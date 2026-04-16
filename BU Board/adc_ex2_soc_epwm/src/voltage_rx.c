/*
 * voltage_rx.c
 *
 * Three-Phase Voltage Reception via CAN
 *
 * Receives voltage data for R, Y, B phases via CAN ID 5
 * Routes frames to separate buffers based on phase identifier
 * Buffer size configurable via CYCLES_TO_CAPTURE in power_config.h
 *
 * Design: Modeled on reference project (BU_S_BOARD11/voltage_can_copy.c)
 *   - Voltage values are extracted and copied to phase buffers DIRECTLY IN THE ISR
 *   - No intermediate rxFIFOMsg[] buffer needed for voltage data
 *   - When all 3 phases complete, flag is set and counters reset immediately
 *   - Main loop only does ping-pong copy and power calculation
 *
 * Frame Format:
 *   Byte 0: Phase identifier (1=R, 2=Y, 3=B)
 *   Byte 1: Frame number (1 to CAN_TOTAL_FRAMES)
 *   Bytes 2-63: 31 voltage values (16-bit each, big-endian)
 *
 * Configuration (power_config.h):
 *   CYCLES_TO_CAPTURE = 2  -> 256 samples/phase, 9 frames/phase, 27 total frames
 *   CYCLES_TO_CAPTURE = 5  -> 640 samples/phase, 21 frames/phase, 63 total frames
 *   CYCLES_TO_CAPTURE = 10 -> 1280 samples/phase, 42 frames/phase, 126 total frames
 */

#include "can_bu.h"

#ifdef CAN_ENABLED

#include "voltage_rx.h"
#include "power_calc.h"
#include "power_calc_3phase.h"  /* For Power3Phase_extractVrmsFromCAN() */
#include <string.h>

/* External state machine variables from adc_ex2_soc_epwm.c */
extern volatile int16_t g_canRxBuffer[];
extern volatile uint16_t g_canFrameCount;
extern volatile uint16_t g_canSampleIndex;
extern volatile bool g_canRxComplete;

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* Three-phase voltage buffers (size based on CYCLES_TO_CAPTURE in power_config.h) */
volatile int16_t g_RPhaseVoltageBuffer[VOLTAGE_BUFFER_SIZE];
volatile int16_t g_YPhaseVoltageBuffer[VOLTAGE_BUFFER_SIZE];
volatile int16_t g_BPhaseVoltageBuffer[VOLTAGE_BUFFER_SIZE];

/* Reception status (legacy - R phase only) */
volatile VoltageRx_Status g_RPhaseRxStatus = {
    .isComplete = false,
    .framesReceived = 0U,
    .framesBitmap = 0U,
    .lastError = 0U
};

/* Three-phase reception status */
volatile VoltageRx_ThreePhaseStatus g_threePhaseRxStatus = {
    .allPhasesComplete = false,
    .rPhaseComplete = false,
    .yPhaseComplete = false,
    .bPhaseComplete = false,
    .rFramesReceived = 0U,
    .yFramesReceived = 0U,
    .bFramesReceived = 0U
};

/* Flag indicating all frames received and ready for processing (set by ISR, cleared by main) */
volatile bool g_framesReady = false;

/* Calibration phase flag */
volatile bool g_calibrationPhase = false;

/* Discovery retry counter and ACK flag */
volatile uint16_t g_discoveryRetryCount = 0;
volatile bool g_discoveryAckRxd = false;

/* Calibration TX retry counter and ACK flag */
volatile uint16_t g_calibTxRetryCount = 0;
volatile bool g_calTxAckRxd = false;

/* Flash command flags - set by ISR, processed by main loop */
volatile bool g_cmdEraseBank4 = false;
volatile uint16_t g_cmdWriteDefaults = CMD_WRITE_DEFAULTS_IDLE;
volatile bool g_cmdSaveCalibration = false;

/*
 * Per-phase frame counters used by ISR to track buffer write position.
 * These are separate from g_threePhaseRxStatus to avoid struct access overhead in ISR.
 */
static volatile uint32_t g_rFrameCount = 0U;
static volatile uint32_t g_yFrameCount = 0U;
static volatile uint32_t g_bFrameCount = 0U;

/*
 * Per-phase frame bitmaps for tracking which frame numbers have been received.
 * Each bit corresponds to a frame number (bit 0 = frame 1, bit 1 = frame 2, etc.)
 * Using bitmaps instead of counters allows frame-number-based positioning,
 * which handles continuous reception correctly even when starting mid-batch.
 */
static volatile uint32_t g_rFrameBitmap = 0U;
static volatile uint32_t g_yFrameBitmap = 0U;
static volatile uint32_t g_bFrameBitmap = 0U;

/* Bitmap value when all frames for a phase have been received */
#define VOLTAGE_FRAME_BITMAP_COMPLETE  ((1U << VOLTAGE_TOTAL_FRAMES) - 1U)

/*
 * Save raw data of last frame per phase for VRMS extraction by main loop.
 * Only 3 frames (one per phase) instead of 27 full frames.
 */
static MCAN_RxBufElement g_lastFrameR;
static MCAN_RxBufElement g_lastFrameY;
static MCAN_RxBufElement g_lastFrameB;
static volatile bool g_lastFrameR_valid = false;
static volatile bool g_lastFrameY_valid = false;
static volatile bool g_lastFrameB_valid = false;

/* Total frame counter for debug visibility */
volatile uint32_t rxFIFOMsg_count = 0;

/* Legacy rxFIFOMsg buffer - kept for compatibility but reduced to 1 element */
MCAN_RxBufElement rxFIFOMsg[1];

/* Debug: Raw bytes from first frame for inspection */
volatile uint8_t g_debugFrame1Data[64];
volatile uint8_t g_debugFrame1Number = 0;
volatile uint32_t g_debugFrame1Received = 0;

/* Debug: First 10 voltage values in different formats */
volatile uint16_t g_debugRawValuesLE[10];   /* Little-endian: low byte first */
volatile uint16_t g_debugRawValuesBE[10];   /* Big-endian: high byte first */
volatile int16_t g_debugValuesWithOffset[10]; /* After -2048 offset */
volatile int16_t g_debugValuesNoOffset[10];   /* Without offset */

/* FIFO status for reading */
MCAN_RxFIFOStatus rxFIFOStatus;

/* ========================================================================== */
/*                    Static Helper Function Declarations                     */
/* ========================================================================== */

static void VoltageRx_processFrame(const MCAN_RxBufElement *msg);

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/**
 * @brief   Initialize the voltage reception module
 */
int32_t VoltageRx_init(uint32_t baseAddr)
{
    /* Reset buffer and status */
    VoltageRx_reset();

    return 0;
}

/**
 * @brief   CAN RX FIFO Interrupt Handler (modeled on reference copyVoltageValuesFromCANFIFO)
 *
 *          Drains the hardware FIFO completely on each call.
 *          For each frame: extracts voltage values and copies DIRECTLY to phase buffers.
 *          When all 3 phases are complete, sets g_framesReady and resets counters
 *          immediately so the ISR is ready for the next batch.
 *
 *          This matches the reference project pattern where all voltage processing
 *          happens in the ISR, not deferred to the main loop.
 */
void VoltageRx_interruptHandler(uint32_t baseAddr)
{
    MCAN_RxBufElement tempMsg;

    /* Get FIFO status */
    rxFIFOStatus.num = MCAN_RX_FIFO_NUM_0;
    MCAN_getRxFIFOStatus(baseAddr, &rxFIFOStatus);

    /* Drain all messages from the FIFO (reference pattern: while fillLvl > 0) */
    while(rxFIFOStatus.fillLvl > 0U)
    {
        /* Read message from FIFO */
        MCAN_readMsgRam(baseAddr,
                       MCAN_MEM_TYPE_FIFO,
                       rxFIFOStatus.getIdx,
                       MCAN_RX_FIFO_NUM_0,
                       &tempMsg);

        /* Acknowledge the FIFO read (advances hardware pointer) */
        MCAN_writeRxFIFOAck(baseAddr, MCAN_RX_FIFO_NUM_0, rxFIFOStatus.getIdx);

        /*
         * ALWAYS process frames (no g_framesReady guard).
         * Frame position is determined by frame NUMBER in the CAN data,
         * so frames always write to the correct buffer position.
         * This ensures continuous reception: frames arriving while main loop
         * is busy simply overwrite the buffer with latest data.
         */
        VoltageRx_processFrame(&tempMsg);

        /* Update FIFO status for next iteration */
        MCAN_getRxFIFOStatus(baseAddr, &rxFIFOStatus);
    }

    /*
     * Check if all phases are complete using bitmaps.
     * Each bit in the bitmap represents a frame number (1-based).
     * When all VOLTAGE_TOTAL_FRAMES bits are set for all 3 phases,
     * one complete batch has been received.
     *
     * Only trigger when g_framesReady is false (prevents re-triggering
     * before main loop has processed the previous batch).
     */
    if(!g_framesReady &&
       (g_rFrameBitmap & VOLTAGE_FRAME_BITMAP_COMPLETE) == VOLTAGE_FRAME_BITMAP_COMPLETE &&
       (g_yFrameBitmap & VOLTAGE_FRAME_BITMAP_COMPLETE) == VOLTAGE_FRAME_BITMAP_COMPLETE &&
       (g_bFrameBitmap & VOLTAGE_FRAME_BITMAP_COMPLETE) == VOLTAGE_FRAME_BITMAP_COMPLETE)
    {
        /* Signal main loop that voltage data is ready */
        g_framesReady = true;

        /* Clear bitmaps for next batch tracking */
        g_rFrameBitmap = 0U;
        g_yFrameBitmap = 0U;
        g_bFrameBitmap = 0U;

        /* Reset debug counters */
        g_rFrameCount = 0U;
        g_yFrameCount = 0U;
        g_bFrameCount = 0U;
        rxFIFOMsg_count = 0U;

        /* Update status */
        g_threePhaseRxStatus.allPhasesComplete = true;
        g_threePhaseRxStatus.rPhaseComplete = true;
        g_threePhaseRxStatus.yPhaseComplete = true;
        g_threePhaseRxStatus.bPhaseComplete = true;
        g_threePhaseRxStatus.rFramesReceived = 0U;
        g_threePhaseRxStatus.yFramesReceived = 0U;
        g_threePhaseRxStatus.bFramesReceived = 0U;
    }
}

/**
 * @brief   Process a single CAN frame: extract voltage values directly to phase buffer
 *          (modeled on reference processVoltageFrameWithCopy)
 *
 *          Key design for continuous operation:
 *          - Frame POSITION in buffer is determined by frame NUMBER from CAN data
 *            (not a sequential counter), so frames always go to the right slot
 *          - Per-phase BITMAPS track which frame numbers have been received
 *          - No g_framesReady guard: frames are always processed, ensuring the
 *            buffer always has the latest data
 *
 * @param   msg - Pointer to the received CAN message
 */
static void VoltageRx_processFrame(const MCAN_RxBufElement *msg)
{
    volatile int16_t *destBuffer;
    uint32_t frameIdx;
    uint32_t phaseBufferIndex;
    uint32_t valuesInFrame;
    uint32_t i;
    uint8_t *byteData;
    uint8_t phaseId;
    uint8_t frameNumber;

    /* Cast uint16_t* to uint8_t* for correct byte access */
    byteData = (uint8_t *)msg->data;

    /* Discovery/stop commands arrive on ID 4 (dedicated RX buffer), handled in can_isr.c.
     * FIFO 0 only carries voltage frames (IDs 5, 6, 7) — no command handling needed here. */

    /* Extract phase ID and frame number from frame header */
    phaseId     = byteData[FRAME_PHASE_BYTE];
    frameNumber = byteData[FRAME_NUMBER_BYTE];

    /* Validate frame number (1-based, sent by S-Board) */
    if(frameNumber < 1U || frameNumber > VOLTAGE_TOTAL_FRAMES)
    {
        return;  /* Invalid frame number, skip */
    }

    /* Convert to 0-based index for buffer positioning */
    frameIdx = (uint32_t)(frameNumber - 1U);

    /* Debug: Capture raw bytes of first frame for inspection */
    if(rxFIFOMsg_count == 0U)
    {
        g_debugFrame1Number = frameNumber;
        g_debugFrame1Received++;
        uint32_t j;
        for(j = 0; j < 64; j++)
        {
            g_debugFrame1Data[j] = byteData[j];
        }
    }

    /* Select destination buffer based on phase and update bitmap */
    if(phaseId == PHASE_ID_R)
    {
        destBuffer = g_RPhaseVoltageBuffer;
        g_rFrameBitmap |= (1U << frameIdx);
        g_rFrameCount++;
        g_threePhaseRxStatus.rFramesReceived++;
    }
    else if(phaseId == PHASE_ID_Y)
    {
        destBuffer = g_YPhaseVoltageBuffer;
        g_yFrameBitmap |= (1U << frameIdx);
        g_yFrameCount++;
        g_threePhaseRxStatus.yFramesReceived++;
    }
    else if(phaseId == PHASE_ID_B)
    {
        destBuffer = g_BPhaseVoltageBuffer;
        g_bFrameBitmap |= (1U << frameIdx);
        g_bFrameCount++;
        g_threePhaseRxStatus.bFramesReceived++;
    }
    else
    {
        /* Unknown phase ID, skip */
        return;
    }

    /* Increment total frame counter for debug */
    rxFIFOMsg_count++;

    /* Calculate buffer write position from frame number */
    phaseBufferIndex = frameIdx * VOLTAGE_VALUES_PER_FRAME;

    /* Calculate values in this frame (last frame may have fewer values) */
    if(frameIdx == (VOLTAGE_TOTAL_FRAMES - 1U))
    {
        valuesInFrame = VOLTAGE_BUFFER_SIZE - phaseBufferIndex;
        if(valuesInFrame > VOLTAGE_VALUES_PER_FRAME)
        {
            valuesInFrame = VOLTAGE_VALUES_PER_FRAME;
        }
    }
    else
    {
        valuesInFrame = VOLTAGE_VALUES_PER_FRAME;
    }

    /* Extract voltage values from CAN frame data directly to phase buffer */
    /* Data starts at byte 2 (bytes 0-1 are phase and frame number) */
    for(i = 0U; i < valuesInFrame; i++)
    {
        if((phaseBufferIndex + i) >= VOLTAGE_BUFFER_SIZE)
        {
            break;  /* Prevent buffer overflow */
        }

        uint32_t bytePos = FRAME_DATA_START_BYTE + (i * 2U);

        /* Big-endian format: high byte first, low byte second */
        uint8_t highByte = byteData[bytePos];
        uint8_t lowByte  = byteData[bytePos + 1U];
        destBuffer[phaseBufferIndex + i] = (int16_t)(((uint16_t)highByte << 8U) | (uint16_t)lowByte) - 2048;
    }

    /*
     * Save last frame data for each phase (for VRMS extraction by main loop)
     * VRMS is in bytes 62-63 of the last CAN frame of each phase
     */
    if(frameIdx == (VOLTAGE_TOTAL_FRAMES - 1U))
    {
        if(phaseId == PHASE_ID_R)
        {
            memcpy(&g_lastFrameR, msg, sizeof(MCAN_RxBufElement));
            g_lastFrameR_valid = true;
        }
        else if(phaseId == PHASE_ID_Y)
        {
            memcpy(&g_lastFrameY, msg, sizeof(MCAN_RxBufElement));
            g_lastFrameY_valid = true;
        }
        else if(phaseId == PHASE_ID_B)
        {
            memcpy(&g_lastFrameB, msg, sizeof(MCAN_RxBufElement));
            g_lastFrameB_valid = true;
        }
    }
}

/**
 * @brief   Reset the voltage reception buffers and status for all 3 phases
 */
void VoltageRx_reset(void)
{
    uint32_t i;

    /* Clear all three phase voltage buffers */
    for(i = 0U; i < VOLTAGE_BUFFER_SIZE; i++)
    {
        g_RPhaseVoltageBuffer[i] = 0;
        g_YPhaseVoltageBuffer[i] = 0;
        g_BPhaseVoltageBuffer[i] = 0;
    }

    /* Reset frame counters and bitmaps */
    g_rFrameCount = 0U;
    g_yFrameCount = 0U;
    g_bFrameCount = 0U;
    g_rFrameBitmap = 0U;
    g_yFrameBitmap = 0U;
    g_bFrameBitmap = 0U;
    rxFIFOMsg_count = 0U;

    /* Reset frames ready flag */
    g_framesReady = false;

    /* Reset last frame validity */
    g_lastFrameR_valid = false;
    g_lastFrameY_valid = false;
    g_lastFrameB_valid = false;

    /* Reset legacy status */
    g_RPhaseRxStatus.isComplete = false;
    g_RPhaseRxStatus.framesReceived = 0U;
    g_RPhaseRxStatus.framesBitmap = 0U;
    g_RPhaseRxStatus.lastError = 0U;

    /* Reset three-phase status */
    g_threePhaseRxStatus.allPhasesComplete = false;
    g_threePhaseRxStatus.rPhaseComplete = false;
    g_threePhaseRxStatus.yPhaseComplete = false;
    g_threePhaseRxStatus.bPhaseComplete = false;
    g_threePhaseRxStatus.rFramesReceived = 0U;
    g_threePhaseRxStatus.yFramesReceived = 0U;
    g_threePhaseRxStatus.bFramesReceived = 0U;
}

/**
 * @brief   Check if all voltage frames have been received
 */
bool VoltageRx_isComplete(void)
{
    return g_RPhaseRxStatus.isComplete;
}

/**
 * @brief   Get pointer to the R-phase voltage buffer (legacy)
 */
volatile uint16_t* VoltageRx_getBuffer(void)
{
    return (volatile uint16_t*)g_RPhaseVoltageBuffer;
}

/**
 * @brief   Check if all three phases are complete
 */
bool VoltageRx_isThreePhaseComplete(void)
{
    return g_threePhaseRxStatus.allPhasesComplete;
}

/**
 * @brief   Get pointer to specific phase voltage buffer
 * @param   phaseId - PHASE_ID_R (1), PHASE_ID_B (2), or PHASE_ID_Y (3)
 * @return  Pointer to the requested phase buffer, or NULL if invalid phase
 */
volatile int16_t* VoltageRx_getPhaseBuffer(uint8_t phaseId)
{
    volatile int16_t* buffer = NULL;

    switch(phaseId)
    {
        case PHASE_ID_R:
            buffer = g_RPhaseVoltageBuffer;
            break;
        case PHASE_ID_Y:
            buffer = g_YPhaseVoltageBuffer;
            break;
        case PHASE_ID_B:
            buffer = g_BPhaseVoltageBuffer;
            break;
        default:
            buffer = NULL;
            break;
    }

    return buffer;
}

/**
 * @brief   Get pointer to three-phase status structure
 */
VoltageRx_ThreePhaseStatus* VoltageRx_getThreePhaseStatus(void)
{
    return (VoltageRx_ThreePhaseStatus*)&g_threePhaseRxStatus;
}

/**
 * @brief   Copy all frames from rxFIFOMsg[] to three-phase voltage buffers (LEGACY)
 *          With the new ISR-direct processing, this function is simplified.
 *          Voltage data is already in the phase buffers. This just updates status.
 */
#pragma CODE_SECTION(VoltageRx_copyToBuffer, ".TI.ramfunc")
void VoltageRx_copyToBuffer(void)
{
    /* Voltage data is already in phase buffers (extracted by ISR) */

    /* Update state machine variables - main code checks g_canRxComplete */
    g_RPhaseRxStatus.framesReceived = VOLTAGE_TOTAL_FRAMES;
    g_canFrameCount = VOLTAGE_TOTAL_FRAMES;
    g_canSampleIndex = VOLTAGE_BUFFER_SIZE;

    /* Clear g_framesReady so ISR can start processing next batch into buffers */
    g_framesReady = false;

    /* Set completion flag */
    g_canRxComplete = true;
}

/**
 * @brief   Copy voltage data to ping-pong buffer for power calculation
 *          Voltage values are ALREADY in the phase buffers (extracted by ISR).
 *          This function:
 *            1. Copies R-phase to ping-pong buffer for legacy power calc
 *            2. Swaps voltage buffer
 *            3. Extracts VRMS from saved last-frame data
 *            4. Sets g_canRxComplete and clears g_framesReady
 *
 * @note    Runs from RAM for faster execution
 */
#pragma CODE_SECTION(VoltageRx_copyToPingPongBuffer, ".TI.ramfunc")
void VoltageRx_copyToPingPongBuffer(void)
{
    volatile int16_t *destBuffer;
    uint32_t i;

    /* Voltage data is already in phase buffers (extracted by ISR) */

    /* Copy R-phase to ping-pong buffer for power calculation (legacy compatibility) */
    destBuffer = PowerCalc_getActiveVoltageBuffer();
    for(i = 0U; i < VOLTAGE_BUFFER_SIZE; i++)
    {
        destBuffer[i] = g_RPhaseVoltageBuffer[i];
    }

    /* Swap the voltage buffer - next cycle will write to the other buffer */
    PowerCalc_swapVoltageBuffer();

    /*
     * Extract VRMS from the saved last CAN frame of each phase (bytes 62-63)
     * The ISR saved the last frame data for each phase in g_lastFrameR/Y/B
     */
    if(g_lastFrameR_valid)
    {
        Power3Phase_extractVrmsFromCAN(g_lastFrameR.data, PHASE_ID_R);
    }
    if(g_lastFrameY_valid)
    {
        Power3Phase_extractVrmsFromCAN(g_lastFrameY.data, PHASE_ID_Y);
    }
    if(g_lastFrameB_valid)
    {
        Power3Phase_extractVrmsFromCAN(g_lastFrameB.data, PHASE_ID_B);
    }

    /* Update status */
    g_RPhaseRxStatus.framesReceived = VOLTAGE_TOTAL_FRAMES;
    g_canFrameCount = VOLTAGE_TOTAL_FRAMES;
    g_canSampleIndex = VOLTAGE_BUFFER_SIZE;

    /*
     * Clear g_framesReady so ISR can start processing next batch into voltage buffers.
     * The ISR checks g_framesReady before calling VoltageRx_processFrame(),
     * so this effectively "unlocks" the voltage buffers for the next batch.
     */
    g_framesReady = false;

    /* Set completion flag - tells main loop CAN data is ready */
    g_canRxComplete = true;
}

#endif /* CAN_ENABLED */
