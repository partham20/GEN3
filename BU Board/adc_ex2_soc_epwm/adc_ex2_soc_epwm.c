//#############################################################################
//
// FILE:   adc_ex2_soc_epwm_dma.c
//
// TITLE:  ADC ePWM Triggering with DMA and DAC Playback
//
// DESCRIPTION:
//   This example implements a capture-then-playback system:
//   1. CAPTURE PHASE: ePWM7 triggers ADC conversions, DMA transfers
//      TOTAL_SAMPLE_COUNT samples to buffer automatically
//   2. PLAYBACK PHASE: After DMA completes, the ADC ISR outputs the
//      captured buffer samples to DAC at the same 5.12kHz rate
//   3. Loop: After playback completes, capture new samples and repeat
//
// EXTERNAL CONNECTIONS:
//   - ADCIN6 should be connected to a signal to convert
//   - DACOUTA outputs the captured/played back signal
//
// WATCH VARIABLES:
//   - myADC0Results[] - Buffer containing ADC conversion results
//   - dacOutput - Current DAC output value
//   - playbackIndex - Current playback position in buffer
//   - isPlaybackMode - Current mode (0=capture, 1=playback)
//
//#############################################################################

//
// Included Files
//
#include "driverlib.h"
#include "device.h"
#include "board.h"
#include "src/power_config.h"  /* Central configuration - must be included before other src headers */
#include "src/can_bu.h"
#include "src/can_isr.h"
#include "src/voltage_rx.h"
#include "src/power_calc.h"
#include "src/ct_dma.h"        /* 18-channel CT DMA module */
#include "src/power_calc_3phase.h"  /* 3-phase power calculation for all 18 CTs */
#include "src/can_branch_tx.h" /* Branch data TX to S-Board (18 CTs) */
#include "src/calibration.h"   /* Per-CT calibration gains (current & KW) */
#include "src/pdu_manager.h"   /* PDU data manager for flash calibration */
#include "src/flash_module.h"  /* Flash API operations for Bank 4 */
#include "src/Firmware Upgrade/fw_update_bu.h"  /* BU firmware OTA receiver */
#include "src/fw_mode.h"        /* System-wide FW update mode control */
#include "metro.h"
//THD
#include "c2000ware_libraries.h"
#include "THD_Module/THD_Module.h"
//
// Defines
//
/* RESULTS_BUFFER_SIZE is now defined in power_config.h as TOTAL_SAMPLE_COUNT */
#define DAC_BASE                DACA_BASE

//
// Globals - Place buffer in dedicated RAM section
//
#pragma DATA_SECTION(myADC0Results, "ramgs0");
uint16_t myADC0Results[NUM_CHANNELS];
uint16_t cpuTimer0IntCount,counter;
extern int16_t ADCResults[NUM_CHANNELS];

/* ── Heartbeat ────────────────────────────────────────────────── */
#define HEARTBEAT_APP_CAN_ID    0x6FFU
static volatile uint32_t g_heartbeatLoopCounter = 0;
#define HEARTBEAT_LOOP_THRESHOLD 750000UL  /* ~5s at tight-loop speed */

static void sendAppHeartbeat(void)
{
    MCAN_TxBufElement txMsg;
    volatile uint32_t timeout;

    memset(&txMsg, 0, sizeof(txMsg));
    txMsg.id  = ((uint32_t)HEARTBEAT_APP_CAN_ID) << 18U;
    txMsg.dlc = 8U;
    txMsg.brs = 0x1U;
    txMsg.fdf = 0x1U;
    txMsg.efc = 1U;
    txMsg.mm  = 0xAAU;
    txMsg.data[0] = 'A';
    txMsg.data[1] = 'P';
    txMsg.data[2] = 'P';
    txMsg.data[3] = 'L';

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 4U, &txMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 4U);
    for (timeout = 0U; timeout < 2000000U; timeout++)
        if ((MCAN_getTxBufReqPend(CAN_BU_BASE) & (1UL << 4U)) == 0U) break;
}

volatile uint16_t done;
volatile bool firstpulse = false;
volatile uint16_t syncLossCounter = 0;

//
// CAN RX Buffer and State Variables
//
volatile int16_t g_canRxBuffer[TOTAL_SAMPLE_COUNT];
volatile uint16_t g_canFrameCount = 0;
volatile uint16_t g_canSampleIndex = 0;
volatile bool g_canRxComplete = false;

//
// DAC/Playback Globals
//
volatile uint16_t dacOutput = 0;
volatile uint16_t adcInput = 0;
volatile uint16_t playbackIndex = 0;
volatile uint16_t isPlaybackMode = 0;  // 0 = capture mode, 1 = playback mode

//
// Power Calculation Globals
//
volatile uint32_t g_cycleCount = 0;              // Number of complete capture/playback cycles
volatile bool g_powerCalcEnabled = true;         // Enable/disable power calculation
volatile int32_t g_lastRealPower = 0;            // Last calculated real power (raw units)

//THD
// ****************************************************************************
// ALIGNED FFT BUFFERS (hardware alignment required by RFFT library)
// ****************************************************************************
#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata1")
    #pragma DATA_ALIGN(1024)
#else
    #pragma DATA_SECTION(RFFTin1Buff,"RFFTdata1")
    #pragma DATA_ALIGN(RFFTin1Buff, 1024)
#endif
float32_t RFFTin1Buff[THD_FFT_SIZE];

#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata2")
#else
    #pragma DATA_SECTION(RFFTmagBuff,"RFFTdata2")
#endif
float32_t RFFTmagBuff[THD_FFT_SIZE/2+1];

#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata3")
#else
    #pragma DATA_SECTION(RFFTphBuff,"RFFTdata3")
#endif
float32_t RFFTphBuff[THD_FFT_SIZE/2];

#ifdef __cplusplus
    #pragma DATA_SECTION("RFFTdata4")
#else
    #pragma DATA_SECTION(RFFToutBuff,"RFFTdata4")
#endif
float32_t RFFToutBuff[THD_FFT_SIZE];

float32_t RFFTTwiddleCoef[THD_FFT_SIZE];
float32_t WindowCoef[THD_FFT_SIZE];

// ****************************************************************************
// GLOBAL POINTERS (required by auto-generated c2000ware_libraries.c)
// ****************************************************************************
float32_t* inPtr      = RFFTin1Buff;
float32_t* outPtr     = RFFToutBuff;
float32_t* magPtr     = RFFTmagBuff;
float32_t* phPtr      = RFFTphBuff;
float32_t* twiddlePtr = RFFTTwiddleCoef;

// ****************************************************************************
// ANALYZER INSTANCE
// ****************************************************************************
THD_Analyzer g_analyzer;

//
// Function Prototypes
//
void initEPWM(void);
void initADC_DMA(void);
void initDAC(void);
void initMCAN(void);
void initCT_DMA(void);
void metrology_main(void);
__interrupt void dmaCh1ISR(void);
__interrupt void dmaCh2ISR(void);
__interrupt void adcA1ISR(void);
__interrupt void cpuTimer0ISR(void);
__interrupt void GPIOISR(void);

//
// ─────────────────────────────────────────────────────────────────
//  Main (SIMPLIFIED — BU FW receive bring-up)
//
//  Stripped-down equivalent of the S-Board's simplified main(). All
//  ADC / DMA / power-calc / THD / voltage RX / branch TX / calibration
//  initialization is disabled. We bring up the bare minimum needed to
//  receive a firmware image from the S-Board:
//
//    1. Device_init  (PLL, flash wait-states, watchdog off)
//    2. FlashAPI_init  (so fw_update_bu real flash path works)
//    3. Interrupt module + vector table
//    4. MCANA on GPIO 4/5  (Launchpad pin layout via FW_BU_MCAN_*_PIN)
//    5. MCANConfig + MCANIntrConfig  (filters incl. 0x30 / 0x31)
//    6. BU_Fw_init  (with FW_BU_BOARD_ID_OVERRIDE if set)
//    7. EINT / ERTM
//
//  Main loop: BU_Fw_process() and a GPIO 20 heartbeat toggle. Nothing
//  else. While BU_Fw_isBusy() the receiver fully owns the CPU — no
//  other tasks run, matching the user requirement that "during a BU
//  firmware upgrade everything else stops."
//
//  When you're ready to restore the full application, comment out
//  this main() and #undef the wrapper around main_FULL() below.
// ─────────────────────────────────────────────────────────────────
//
void main(void)
{
    uint16_t i;

    /* GPIO 29 heartbeat LED */
    GPIO_setPinConfig(GPIO_29_GPIO29);
    GPIO_setDirectionMode(29, GPIO_DIR_MODE_OUT);
    GPIO_setPadConfig(29, GPIO_PIN_TYPE_STD);

    /* ── 1. Core init + PIE + FPU libraries ─────────────────── */
    Device_init();
    Device_initGPIO();
    FlashAPI_init();
    Interrupt_initModule();
    Interrupt_initVectorTable();
    C2000Ware_libraries_init();

    /* ── 2. THD analyzer (FFT + window tables) ──────────────── */
    THD_init(&g_analyzer,
             RFFTin1Buff,
             RFFToutBuff,
             RFFTmagBuff,
             RFFTphBuff,
             RFFTTwiddleCoef,
             WindowCoef,
             (float32_t*)g_adcState.rawBuffers,
             THD_NUM_CHANNELS,
             THD_FFT_SIZE,
             THD_FFT_STAGES);
    THD_ADC_init();

    /* ── 3. MCAN clock + board-level SysConfig init ─────────── */
    SysCtl_setMCANClk(CAN_BU_SYSCTL, SYSCTL_MCANCLK_DIV_5);
    Board_init();

    /* ── 4. Metrology + MCAN + BU FW receiver ───────────────── */
    metrology_main();
    initMCAN();
    {
        uint8_t myFwId = (uint8_t)(10U + address);
#if defined(FW_BU_BOARD_ID_OVERRIDE) && (FW_BU_BOARD_ID_OVERRIDE != 0)
        myFwId = (uint8_t)FW_BU_BOARD_ID_OVERRIDE;
#endif
        BU_Fw_init(myFwId);
    }

    /* ── 5. Power calc + calibration + branch TX ────────────── */
    PowerCalc_init();
    Power3Phase_init();
    Calibration_init();

#ifdef CAN_ENABLED
    {
        bool flashOk = initializeFlashAndLoadData();
        if (flashOk) { PDU_syncToCalibration(); }
    }
#endif

    BranchTx_init();

    /* ── 6. ADC / CT DMA / DAC / DMA ISRs / ePWM ────────────── */
    initADC_DMA();
    initCT_DMA();
    initDAC();
    Interrupt_register(INT_DMA_CH1, &dmaCh1ISR);
    Interrupt_enable(INT_DMA_CH1);
    Interrupt_register(INT_DMA_CH2, &dmaCh2ISR);
    Interrupt_enable(INT_DMA_CH2);

    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    initEPWM();
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    /* Zero ADC results buffer before first capture */
    for (i = 0; i < NUM_CHANNELS; i++) { myADC0Results[i] = 0; }

    /* ── 7. Global interrupts on ───────────────────────────── */
    EINT;
    ERTM;

    /* ── 8. System-wide FW update mode state machine ────────── */
    FwMode_init();
    FwMode_sendBootStatus();

    /* ══════════════════════════════════════════════════════════
     *  Main loop
     * ══════════════════════════════════════════════════════════ */
    while (1)
    {
        /* BU firmware OTA has absolute priority — stop everything
         * else while it's actively receiving/flashing. */
        if (BU_Fw_isBusy())
        {
            BU_Fw_process();
            continue;
        }
        BU_Fw_process();

        /* FW-mode tick (drain ENTER/EXIT/STATUS_REQ + 1 Hz heartbeat) */
        FwMode_tick();

        /* In FW update mode everything normal is suspended. */
        if (FwMode_isActive()) continue;

#ifdef CAN_ENABLED
        /* ── Flash command processing (calibration bucket) ─── */
        if (g_cmdEraseBank4)
        {
            DINT;
            eraseBank4();
            EINT;
            newestData = 0; oldestData = 0xFFFF;
            writeAddress = 0xFFFFFFFF;
            addressToWrite = BANK4_START;
            emptySectorFound = true;
            CAN_sendErasedBank4Ack();
            g_cmdEraseBank4 = false;
        }

        if (g_cmdWriteDefaults == CMD_WRITE_DEFAULTS_KEY)
        {
            g_cmdWriteDefaults = CMD_WRITE_DEFAULTS_IDLE;
            DINT;
            findReadAndWriteAddress();
            if (newestData >= WRAP_LIMIT)
            {
                eraseBank4();
                newestData = 0; oldestData = 0xFFFF;
                writeAddress = 0xFFFFFFFF;
                addressToWrite = BANK4_START;
                emptySectorFound = true;
            }
            else { eraseSectorOrFindEmptySector(); }
            writeDefaultCalibrationValues();
            writeToFlash();
            EINT;
            if (!updateFailed)
            {
                syncReadWriteToWorking();
                PDU_syncToCalibration();
                CAN_sendWrittenDefaultsAck();
            }
        }

        if (g_cmdSaveCalibration)
        {
            syncWorkingToReadWrite();
            DINT;
            findReadAndWriteAddress();
            if (newestData >= WRAP_LIMIT)
            {
                eraseBank4();
                newestData = 0; oldestData = 0xFFFF;
                writeAddress = 0xFFFFFFFF;
                addressToWrite = BANK4_START;
                emptySectorFound = true;
            }
            else { eraseSectorOrFindEmptySector(); }
            pduManager.readWriteData.sectorIndex = ++newestData;
            pduManager.readWriteData.BUBoardID = address;
            memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibData));
            datasize = sizeof(CalibData) / sizeof(uint16_t);
            writeToFlash();
            EINT;
            if (!updateFailed) { CAN_sendCalibUpdateSuccessAck(); }
            else               { CAN_sendCalibUpdateFailAck();     }
            g_cmdSaveCalibration = false;
        }

        /* Auto-save calibration frames from S-Board */
        if (g_calibRx.dataReady)
        {
            uint16_t ct;
            pduManager.readWriteData.BUBoardID = address;
            for (ct = 0; ct < CAL_NUM_CTS; ct++)
            {
                pduManager.readWriteData.currentGain[ct] = g_calibRx.currentGain[ct];
                pduManager.readWriteData.kwGain[ct]      = g_calibRx.kwGain[ct];
            }
            DINT;
            findReadAndWriteAddress();
            if (newestData >= WRAP_LIMIT)
            {
                eraseBank4();
                newestData = 0; oldestData = 0xFFFF;
                writeAddress = 0xFFFFFFFF;
                addressToWrite = BANK4_START;
                emptySectorFound = true;
            }
            else { eraseSectorOrFindEmptySector(); }
            pduManager.readWriteData.sectorIndex = ++newestData;
            memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibData));
            datasize = sizeof(CalibData) / sizeof(uint16_t);
            writeToFlash();
            EINT;
            if (!updateFailed)
            {
                syncReadWriteToWorking();
                PDU_syncToCalibration();
                CAN_sendCalibUpdateSuccessAck();
            }
            else { CAN_sendCalibUpdateFailAck(); }
            g_calibRx.currentRxd = false;
            g_calibRx.kwRxd      = false;
            g_calibRx.dataReady  = false;
        }

        /* Skip runtime ADC/DMA work while calibrating. */
        if (g_calibrationPhase) { continue; }
#endif

        /* ── STEP 1: wait for sync pulse (GPIO28 from S-Board) ── */
        while (firstpulse == false)
        {
#ifdef CAN_ENABLED
            if (g_calibrationPhase) break;
#endif
            /* Still honour FW-mode transitions while waiting. */
            FwMode_tick();
            if (FwMode_isActive()) break;
        }
        if (FwMode_isActive()) continue;

        /* ── STEP 2: CT DMA capture (18 channels) ── */
        done = 0; playbackIndex = 0; isPlaybackMode = 1;
        ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER2);
        ADC_clearInterruptStatus(ADCE_BASE,   ADC_INT_NUMBER2);
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER2);
        ADC_clearInterruptOverflowStatus(ADCE_BASE,   ADC_INT_NUMBER2);
        EPWM_clearADCTriggerFlag(EPWM7_BASE, EPWM_SOC_A);
        EPWM_forceADCTriggerEventCountInit(EPWM7_BASE, EPWM_SOC_A);

        CT_DMA_reset();
        CT_DMA_start();
        EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

        /* STEP 3: wait for DMA completion (both channels). */
        while (!CT_DMA_isComplete() && firstpulse == true)
        {
#ifdef CAN_ENABLED
            if (g_calibrationPhase) break;
#endif
        }

        EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);
        CT_DMA_copyToPingPong();
        CT_DMA_swapPingPong();

        {
            volatile int16_t* ct1Inactive = CT_DMA_getInactiveCTBuffer(1);
            PowerCalc_copyCurrentFromADC((uint16_t*)ct1Inactive, RESULTS_BUFFER_SIZE);
            PowerCalc_swapCurrentBuffer();
        }

        /* ── Playback phase ── */
        done = 0; playbackIndex = 0; isPlaybackMode = 1;
        ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
        EPWM_clearADCTriggerFlag(EPWM7_BASE, EPWM_SOC_A);

        counter++;
        EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);
        while (done == 0)
        {
#ifdef CAN_ENABLED
            if (g_calibrationPhase) break;
#endif
        }
        EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

#ifdef CAN_ENABLED
        /* STEP 4: wait for voltage RX from S-Board. */
        while (g_canRxComplete == false)
        {
            if (g_framesReady)
            {
                VoltageRx_copyToPingPongBuffer();
                g_framesReady = false;
            }
            if (g_calibrationPhase) break;
        }
        g_canRxComplete = false;

        Power3Phase_copyVoltageToActive(POWER_PHASE_R,
                                          g_RPhaseVoltageBuffer, TOTAL_SAMPLE_COUNT);
        Power3Phase_copyVoltageToActive(POWER_PHASE_Y,
                                          g_YPhaseVoltageBuffer, TOTAL_SAMPLE_COUNT);
        Power3Phase_copyVoltageToActive(POWER_PHASE_B,
                                          g_BPhaseVoltageBuffer, TOTAL_SAMPLE_COUNT);
        Power3Phase_swapAllVoltageBuffers();
#endif

        /* STEP 5: power calc (18 CTs). */
        if (g_powerCalcEnabled && g_cycleCount > 0)
        {
            Power3Phase_calculateAllPower();
            g_lastRealPower = Power3Phase_getTotalPower_raw();
        }
        g_cycleCount++;

        App_calculateMetrologyParameters(&gAppHandle, &gMetrologyWorkingData);

        if (g_powerCalcEnabled && g_cycleCount > 1)
        {
            Power3Phase_calculateApparentReactivePF();
        }

        if (THD_ADC_isBufferReady())
        {
            THD_processAllChannels(&g_analyzer);
            THD_ADC_resetAndRearm();
        }

        /* STEP 7: branch TX to S-Board (18 frames). */
        if (g_powerCalcEnabled && g_cycleCount > 1)
        {
            uint16_t ct;
            for (ct = 1; ct <= BRANCH_TX_NUM_CTS; ct++)
            {
                float irms = gMetrologyWorkingData.phases[ct - 1]
                              .readings.RMSCurrent
                             * g_calibration.currentGain[ct - 1];
                const CT_PowerResult *ctResult;
                float realPower = 0.0f;
                uint16_t ctIndex   = ct - 1;
                uint16_t phase     = ctIndex / POWER_CTS_PER_PHASE;
                uint16_t ctInPhase = ctIndex % POWER_CTS_PER_PHASE;

                if      (phase == POWER_PHASE_R) ctResult = &g_power3PhaseResults.rPhase.ct[ctInPhase];
                else if (phase == POWER_PHASE_Y) ctResult = &g_power3PhaseResults.yPhase.ct[ctInPhase];
                else                              ctResult = &g_power3PhaseResults.bPhase.ct[ctInPhase];
                realPower = ctResult->realPower_watts;
                BranchTx_updateStats(ct, irms, realPower);
            }
            BranchTx_sendAllBranches();
        }

        /* Normal app heartbeat (0x6FF) — suppressed in FW mode */
        g_heartbeatLoopCounter++;
        if (g_heartbeatLoopCounter >= HEARTBEAT_LOOP_THRESHOLD)
        {
            g_heartbeatLoopCounter = 0;
            sendAppHeartbeat();
            GPIO_togglePin(29);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  Suspend / resume hooks — called by fw_mode.c on ENTER / EXIT.
 *
 *  Stops the BU real-time chain: ePWM7 (5.12 kHz SOC trigger),
 *  CT DMA (18 channels), ADC-level interrupts, CPU timer.
 *
 *  Register writes against uninitialised peripherals are NOPs —
 *  today's simplified main() (BU_Fw_process only) doesn't start
 *  them, so these calls have no effect until main_FULL is
 *  restored.  At that point the hooks will do the right thing.
 * ═══════════════════════════════════════════════════════════════ */
void buBoardSuspendNormalMode(void)
{
    /* 1. Freeze the ePWM7 SOC trigger so ADC stops converting. */
    EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);
    EPWM_setTimeBaseCounterMode(EPWM7_BASE,
                                 EPWM_COUNTER_MODE_STOP_FREEZE);

    /* 2. Stop the CT DMA capture (both channels). */
    CT_DMA_stop();

    /* 3. Disable ADC interrupts so any stale EOCs don't fire. */
    ADC_disableInterrupt(myADC0_BASE, ADC_INT_NUMBER1);
    ADC_disableInterrupt(myADC0_BASE, ADC_INT_NUMBER2);
    ADC_disableInterrupt(myADC1_BASE, ADC_INT_NUMBER2);   /* ADC-E INT2 */

    /* 4. Disable PIE-level interrupts for ADC + DMA ISRs. */
    Interrupt_disable(INT_ADCA1);
    Interrupt_disable(INT_DMA_CH1);
    Interrupt_disable(INT_DMA_CH2);

    /* 5. Stop the CPU timer driving calibration phase timing. */
    CPUTimer_stopTimer(CPUTIMER0_BASE);
}

void buBoardResumeNormalMode(void)
{
    /* 1. Clear stale ADC interrupt flags before re-arming. */
    ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
    ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER2);
    ADC_clearInterruptStatus(myADC1_BASE, ADC_INT_NUMBER2);

    /* 2. Reset CT DMA to a clean state (clear triggers + addresses). */
    CT_DMA_reset();

    /* 3. Re-enable ADC interrupts. */
    ADC_enableInterrupt(myADC0_BASE, ADC_INT_NUMBER1);
    ADC_enableInterrupt(myADC0_BASE, ADC_INT_NUMBER2);
    ADC_enableInterrupt(myADC1_BASE, ADC_INT_NUMBER2);

    /* 4. Re-enable PIE interrupts. */
    Interrupt_enable(INT_ADCA1);
    Interrupt_enable(INT_DMA_CH1);
    Interrupt_enable(INT_DMA_CH2);

    /* 5. Restart the CPU timer. */
    CPUTimer_startTimer(CPUTIMER0_BASE);

    /* 6. Main loop's capture cycle will re-start DMA + ePWM trigger
     *    as part of its next iteration — nothing else to do here. */
}

/* ═══════════════════════════════════════════════════════════════
 *  Original full application main — disabled while bringing up the
 *  BU firmware OTA receiver. To restore: change `#if 0` to `#if 1`
 *  AND rename the simplified main() above to something else (e.g.
 *  main_BU_FW_ONLY) so the linker picks the right entry point.
 * ═══════════════════════════════════════════════════════════════ */
#if 0
//
// Main
//
void main_FULL(void)
{
    uint16_t i;

    //
    // Initialize device clock and peripherals CAL_TX_GAIN_SCALE)
    //
    Device_init();

    //
    // Disable pin locks and enable internal pullups WRAP_LIMIT
    //
    Device_initGPIO();

    //
    // Initialize PIE and clear PIE registers. Disables CPU interrupts.
    //
    Interrupt_initModule();

    //
    // Initialize the PIE vector table with pointers to the shell ISRs
    //
    Interrupt_initVectorTable();
    //THD
    C2000Ware_libraries_init();

    // ---- Initialize THD Analyzer ----
    THD_init(&g_analyzer,
             RFFTin1Buff,
             RFFToutBuff,
             RFFTmagBuff,
             RFFTphBuff,
             RFFTTwiddleCoef,
             WindowCoef,
             (float32_t*)g_adcState.rawBuffers,
             THD_NUM_CHANNELS,
             THD_FFT_SIZE,
             THD_FFT_STAGES);

    // ---- Start ADC Acquisition ----
    THD_ADC_init();
    //THD_ADC_initEPWM();
    //THD_ADC_start();

    //
    // Configure MCAN clock divider BEFORE Board_init() calls MCAN_init()
    // SYSCLK = 150MHz, /5 = 30MHz MCAN clock
    // Nominal baud = 30MHz / (12 * 5) = 500 kbps
    // Data baud = 30MHz / (3 * 5) = 2 Mbps
    // NOTE: Divider MUST match S-Board's MCAN clock configuration!
    //
    SysCtl_setMCANClk(CAN_BU_SYSCTL, SYSCTL_MCANCLK_DIV_5);

    //
    // Board Initialization (ADC, DMA base config from SysConfig)
    //
    Board_init();

    //
    // Override cpuTimer0 period for sync loss detection
    // SysConfig sets 23437 (~0.16ms at 150MHz) which is too fast for 160ms sync period
    // Set to 100ms (15,000,000 cycles at 150MHz)
    // With threshold > 2: sync loss declared after 300ms (~2 missed sync periods)
    //
//    CPUTimer_stopTimer(myCPUTIMER0_BASE);
//    CPUTimer_setPeriod(myCPUTIMER0_BASE, 15000000U - 1U);
//    CPUTimer_reloadTimerCounter(myCPUTIMER0_BASE);
//    CPUTimer_startTimer(myCPUTIMER0_BASE);

    metrology_main();
    //
    // Initialize MCAN (CAN-FD) for voltage data reception
    //
    initMCAN();

    //
    // Initialize BU firmware OTA receiver. Must happen AFTER initMCAN()
    // so that `address` is already populated from the DIP switches —
    // BU_Fw_init() uses it to filter incoming FW frames by target BU id.
    //
    // On a Launchpad dev board there are no DIP switches so `address`
    // reads as 0, which breaks the target-id filter (S-Board sends
    // 11..22). fw_upgrade_config.h offers FW_BU_BOARD_ID_OVERRIDE — if
    // nonzero, use it instead of the DIP switch value.
    //
    {
        uint8_t myFwId = (uint8_t)(10U + address);  /* CAN ID = 10 + DIP switch */
#if defined(FW_BU_BOARD_ID_OVERRIDE) && (FW_BU_BOARD_ID_OVERRIDE != 0)
        myFwId = (uint8_t)FW_BU_BOARD_ID_OVERRIDE;
#endif
        BU_Fw_init(myFwId);
    }

    //
    // Initialize power calculation module (ping-pong buffers)
    //
    PowerCalc_init();

    //
    // Initialize 3-phase power calculation module (18 CT power calculation)
    //
    Power3Phase_init();

    //
    // Initialize per-CT calibration gains (current & KW)
    // Sets all 18 gains to default 1.0f
    //
    Calibration_init();

    //
    // Initialize flash and load calibration data from Bank 4
    // If flash has stored calibration, overrides the 1.0f defaults above
    // If flash is empty, writes default calibration (885/9677) to flash
    //
#ifdef CAN_ENABLED
    {
        bool flashOk = initializeFlashAndLoadData();
        if (flashOk)
        {
            // Convert uint16_t flash gains to float and set g_calibration
            PDU_syncToCalibration();
        }
    }
#endif

    //
    // Initialize branch TX module for sending data to S-Board
    // Sends 18 CAN frames (1 frame per CT, 46 bytes each)
    //
    BranchTx_init();

    //
    // Configure ADC and DMA for proper operation (legacy single channel)
    //
    initADC_DMA();

    //
    // Initialize 18-channel CT DMA module
    //
    initCT_DMA();

    //
    // Initialize and configure DAC
    //
    initDAC();

    //
    // Register and enable DMA Ch1 interrupt (for 18-CT ADC A)
    //
    Interrupt_register(INT_DMA_CH1, &dmaCh1ISR);
    Interrupt_enable(INT_DMA_CH1);

    //
    // Register and enable DMA Ch2 interrupt (for 18-CT ADC E)
    //
    Interrupt_register(INT_DMA_CH2, &dmaCh2ISR);
    Interrupt_enable(INT_DMA_CH2);

    //
    // Stop the ePWM clock before configuration
    //
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    //
    // Set up the ePWM
    //
    initEPWM();

    //
    // Start the ePWM clock
    //
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);

    //
    // Initialize results buffer to zero
    //
    for(i = 0; i < NUM_CHANNELS; i++)
    {
        myADC0Results[i] = 0;
    }

    //
    // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
    //
    EINT;
    ERTM;

    //
    // Loop indefinitely - Capture and Playback cycle
    //
    while(1)
    {
        //
        // ==================== BU FIRMWARE OTA (HIGHEST PRIORITY) ========
        // When the S-Board is pushing a firmware image to this BU, we
        // STOP EVERYTHING ELSE: no ADC processing, no power calc, no
        // THD, no calibration. Flash writes cannot be safely interleaved
        // with the 5.12 kHz ADC/DMA loop, and the sender streams at full
        // CAN-FD rate so we need all the CPU we can get.
        //
        // Once BU_Fw_isBusy() goes false (ACTIVATE done → reset, or
        // ABORT processed), normal ops resume automatically on the
        // next loop iteration.
        //
        if (BU_Fw_isBusy())
        {
            BU_Fw_process();
            continue;
        }
        /* Always give the receiver a chance to pick up a PREPARE
         * command that arrives while we are in normal ops. The first
         * PREPARE transitions the state to PREPARING and from then on
         * the guard above takes over. */
        BU_Fw_process();

        //
        // ==================== FLASH COMMAND PROCESSING ====================
        // Process flash commands received via CAN during calibration phase
        // These run in the main loop context (not ISR) because flash ops are blocking
        //
#ifdef CAN_ENABLED
        if (g_cmdEraseBank4)
        {
            // Command 0x09: Erase entire Bank 4
            DINT;  // Disable interrupts during flash erase
            eraseBank4();
            EINT;  // Re-enable interrupts
            newestData = 0;
            oldestData = 0xFFFF;
            writeAddress = 0xFFFFFFFF;
            addressToWrite = BANK4_START;
            emptySectorFound = true;
            CAN_sendErasedBank4Ack();
            g_cmdEraseBank4 = false;
        }

        if (g_cmdWriteDefaults == CMD_WRITE_DEFAULTS_KEY)
        {
            // Command 0x01: Write default calibration values to flash
            // Clear immediately with magic value to prevent re-entry from corruption
            g_cmdWriteDefaults = CMD_WRITE_DEFAULTS_IDLE;

            DINT;  // Disable interrupts for ALL flash operations (ISRs run from flash)
            findReadAndWriteAddress();
            // Wrap check: if limit reached, erase bank and start fresh
            if (newestData >= WRAP_LIMIT)
            {
                eraseBank4();
                newestData = 0;
                oldestData = 0xFFFF;
                writeAddress = 0xFFFFFFFF;
                addressToWrite = BANK4_START;
                emptySectorFound = true;
            }
            else
            {
                eraseSectorOrFindEmptySector();
            }
            writeDefaultCalibrationValues();
            writeToFlash();
            EINT;
            if (!updateFailed)
            {
                syncReadWriteToWorking();
                PDU_syncToCalibration();
                CAN_sendWrittenDefaultsAck();
            }
        }

        if (g_cmdSaveCalibration)
        {
            // Command 0x04: Save current runtime calibration to flash
            // pduManager.workingData already has exact uint16_t values from CAN ISR
            syncWorkingToReadWrite();
            DINT;  // Disable interrupts for ALL flash operations (ISRs run from flash)
            findReadAndWriteAddress();
            // Wrap check: if limit reached, erase bank and start fresh
            if (newestData >= WRAP_LIMIT)
            {
                eraseBank4();
                newestData = 0;
                oldestData = 0xFFFF;
                writeAddress = 0xFFFFFFFF;
                addressToWrite = BANK4_START;
                emptySectorFound = true;
            }
            else
            {
                eraseSectorOrFindEmptySector();
            }
            // Increment AFTER findReadAndWriteAddress so newestData is fresh from flash
            pduManager.readWriteData.sectorIndex = ++newestData;
            pduManager.readWriteData.BUBoardID = address;
            memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibData));
            datasize = sizeof(CalibData) / sizeof(uint16_t);
            writeToFlash();
            EINT;
            if (!updateFailed)
            {
                CAN_sendCalibUpdateSuccessAck();
            }
            else
            {
                CAN_sendCalibUpdateFailAck();
            }
            g_cmdSaveCalibration = false;
        }

        //
        // ==================== AUTO-SAVE CALIBRATION FROM S-BOARD ====================
        // When both calibration frames (current + kW gains) received via CAN ISR,
        // copy to pduManager, write to flash, and sync to runtime calibration.
        //
        if (g_calibRx.dataReady)
        {
            // Copy parsed gains from ISR temp struct to pduManager
            uint16_t ct;
            pduManager.readWriteData.BUBoardID = address;
            for (ct = 0; ct < CAL_NUM_CTS; ct++)
            {
                pduManager.readWriteData.currentGain[ct] = g_calibRx.currentGain[ct];
                pduManager.readWriteData.kwGain[ct]      = g_calibRx.kwGain[ct];
            }

            // Flash write sequence
            DINT;
            findReadAndWriteAddress();
            if (newestData >= WRAP_LIMIT)
            {
                eraseBank4();
                newestData = 0;
                oldestData = 0xFFFF;
                writeAddress = 0xFFFFFFFF;
                addressToWrite = BANK4_START;
                emptySectorFound = true;
            }
            else
            {
                eraseSectorOrFindEmptySector();
            }
            pduManager.readWriteData.sectorIndex = ++newestData;
            memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibData));
            datasize = sizeof(CalibData) / sizeof(uint16_t);
            writeToFlash();
            EINT;

            if (!updateFailed)
            {
                // Sync to workingData and runtime float calibration
                syncReadWriteToWorking();
                PDU_syncToCalibration();
                CAN_sendCalibUpdateSuccessAck();
            }
            else
            {
                CAN_sendCalibUpdateFailAck();
            }

            // Reset for next calibration cycle
            g_calibRx.currentRxd = false;
            g_calibRx.kwRxd      = false;
            g_calibRx.dataReady  = false;
        }

        //
        // During calibration phase, skip ALL normal operation.
        // Only flash commands (above) and CAN ISR-based calibration RX run.
        // Normal operation resumes after 0x06 (CPU reset) ends calibration.
        //
        if (g_calibrationPhase)
        {
            continue;
        }
#endif

        //
        // ==================== STEP 1: WAIT FOR SYNC SIGNAL ====================
        // Do not start until sync pulse is received from S board (GPIO28)
        //
        while(firstpulse == false)
        {
#ifdef CAN_ENABLED
            if (g_calibrationPhase)
            {
                break;
            }
#endif
        }

        //
        // ==================== STEP 2: ADC/DMA CAPTURE PHASE (18 CT Channels) ====================
        //
//        done = 0;
//        isPlaybackMode = 0;  // Set to capture mode
        done = 0;
        playbackIndex = 0;
        isPlaybackMode = 1;  // Set to playback mode
        //
        // Clear all pending interrupt flags
        //
        ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER2);
        ADC_clearInterruptStatus(ADCE_BASE, ADC_INT_NUMBER2);
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER2);
        ADC_clearInterruptOverflowStatus(ADCE_BASE, ADC_INT_NUMBER2);
        EPWM_clearADCTriggerFlag(EPWM7_BASE, EPWM_SOC_A);
        EPWM_forceADCTriggerEventCountInit(EPWM7_BASE, EPWM_SOC_A);

        //
        // Reset and start 18-channel CT DMA
        //
        CT_DMA_reset();
        CT_DMA_start();

        //
        // Enable ADC trigger - EPWM is already started by GPIOISR
        //
        EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

        //
        // ==================== STEP 3: WAIT FOR DMA BUFFER COMPLETE ====================
        // Wait for both DMA channels (ADC A and ADC E) to complete
        // Also exit if sync is lost
        //
        while(!CT_DMA_isComplete() && firstpulse == true)
        {
#ifdef CAN_ENABLED
            if (g_calibrationPhase) break;
#endif
        }

        //
        // If sync lost during capture, disable trigger and restart
        //
//        if(firstpulse == false)
//        {
//            EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);
//            CT_DMA_stop();
//            continue;
//        }

        //
        // DMA capture complete - Disable ADC trigger temporarily
        //
        EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

        //
        // Capture succeeded - stop sync loss detection for processing phase
        // S-board also stops sending after its 40ms burst, so no GPIO28 edges
        // will arrive. Without this, timer kills firstpulse in 0.47ms.
        // Timer will resume checking when we loop back to wait for next sync.
        //
     //   firstpulse = false;

        //
        // ==================== COPY CT DATA TO PING-PONG BUFFERS ====================
        // Copy raw ADC results to active ping-pong buffer for all 18 CTs
        // DC offset (CT_DC_OFFSET = 2010) is removed during copy
        // Data organized by phase: R (CT1-6), Y (CT7-12), B (CT13-18)
        //
        CT_DMA_copyToPingPong();

        //
        // Swap ping-pong buffers - makes current data available for processing
        // while next cycle writes to the other buffer
        //
        CT_DMA_swapPingPong();

        //
        // ==================== COPY CURRENT TO POWER CALC BUFFER ====================
        // Copy CT1 (first R-phase CT) from inactive buffer for power calculation
        // TODO: Update to sum all 6 CTs per phase for total phase current
        //
        {
            volatile int16_t* ct1Inactive = CT_DMA_getInactiveCTBuffer(1);
            PowerCalc_copyCurrentFromADC((uint16_t*)ct1Inactive, RESULTS_BUFFER_SIZE);
            PowerCalc_swapCurrentBuffer();
        }

        //
        // ==================== PLAYBACK PHASE ====================
        //
        done = 0;
        playbackIndex = 0;
        isPlaybackMode = 1;  // Set to playback mode

        //
        // Clear interrupt flags before playback
        //
        ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
        EPWM_clearADCTriggerFlag(EPWM7_BASE, EPWM_SOC_A);

        counter++;
        //
        // Enable ADC trigger for playback - EPWM already running from sync
        //
        EPWM_enableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

        //
        // Wait for playback to complete (done flag set by adcA1ISR)
        // Also exit if sync is lost
        //
        while(done == 0)
        {
#ifdef CAN_ENABLED
            if (g_calibrationPhase) break;
#endif
        }

        //
        // Playback complete or sync lost - Disable ADC trigger
        //
        EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

        //
        // ==================== STEP 4: WAIT FOR CAN RX FRAMES ====================
        // Wait for 42 CAN frames to be received, then process in main loop (not ISR)
        //
#ifdef CAN_ENABLED
        while(g_canRxComplete == false)
        {
            // Check if all frames received and ready for processing
            if(g_framesReady)
            {
                // Process frames and copy to ping-pong voltage buffer
                // Note: g_framesReady is cleared inside VoltageRx_copyToPingPongBuffer()
                // atomically with rxFIFOMsg_count reset to prevent race conditions
                VoltageRx_copyToPingPongBuffer();
                g_framesReady = false;
            }
            if (g_calibrationPhase) break;
        }

        //
        // CAN data received - clear flag for next cycle
        //
        g_canRxComplete = false;

        //
        // ==================== COPY VOLTAGE TO 3-PHASE POWER CALC BUFFERS ====================
        // Copy received voltage data from voltage_rx buffers to power_calc_3phase ping-pong buffers
        // This makes voltage data available for power calculation with all 18 CTs
        //
        Power3Phase_copyVoltageToActive(POWER_PHASE_R, g_RPhaseVoltageBuffer, TOTAL_SAMPLE_COUNT);
        Power3Phase_copyVoltageToActive(POWER_PHASE_Y, g_YPhaseVoltageBuffer, TOTAL_SAMPLE_COUNT);
        Power3Phase_copyVoltageToActive(POWER_PHASE_B, g_BPhaseVoltageBuffer, TOTAL_SAMPLE_COUNT);

        //
        // Swap voltage buffers - makes current data available for processing
        //
        Power3Phase_swapAllVoltageBuffers();
#endif

        //
        // ==================== STEP 5: POWER CALCULATION (ALL 18 CTs) ====================
        // Calculate real power for all 18 CTs from inactive buffers (previous cycle data)
        // P_real[ct] = (1/N) * SUM(V_phase[i] * I_ct[i])
        //
        // CT to Phase Mapping:
        //   CT1-CT6   use R-Phase voltage
        //   CT7-CT12  use Y-Phase voltage
        //   CT13-CT18 use B-Phase voltage
        //
        // Note: First cycle will use uninitialized data - results valid from cycle 2
        //
        if(g_powerCalcEnabled && g_cycleCount > 0)
        {
            // Calculate real power for all 18 CTs using 3-phase voltage and current buffers
            Power3Phase_calculateAllPower();

            // Store last calculated total system power for monitoring (all 18 CTs combined)
            g_lastRealPower = Power3Phase_getTotalPower_raw();
        }

        //
        // Increment cycle counter
        //
        g_cycleCount++;

        //
        // Calculate metrology parameters (IRMS for each CT)
        //
        App_calculateMetrologyParameters(&gAppHandle, &gMetrologyWorkingData);

        //
        // ==================== STEP 6: APPARENT, REACTIVE POWER & PF CALCULATION ====================
        // Calculate apparent power, reactive power and power factor for all 18 CTs
        // Uses: VRMS from CAN (extracted in VoltageRx_copyToPingPongBuffer)
        //       IRMS from gMetrologyWorkingData.phases[ctIndex].readings.RMSCurrent
        //       Real power from Power3Phase_calculateAllPower()
        //
        // Formulas:
        //   S (Apparent Power) = Vrms * Irms [VA]
        //   Q (Reactive Power) = sqrt(S^2 - P^2) [VAr]
        //   PF (Power Factor) = P / S [unitless]
        //
        // Note: Must be called AFTER both Power3Phase_calculateAllPower() and
        //       App_calculateMetrologyParameters() to have valid P and Irms values
        //
        if(g_powerCalcEnabled && g_cycleCount > 1)
        {
            Power3Phase_calculateApparentReactivePF();
        }
        //THD
        if(THD_ADC_isBufferReady())
        {
            THD_processAllChannels(&g_analyzer);
            THD_ADC_resetAndRearm();
        }
        //
        // ==================== STEP 7: SEND BRANCH DATA TO S-BOARD ====================
        // Send branch data for all 18 CTs via CAN-FD
        // Each CT sends 1 frame (46 bytes) containing:
        //   - Version, S-Board ID, BU-Board ID, CT Number
        //   - Current (IRMS, Max, Min, Load%)
        //   - THD (Current, Voltage)
        //   - Power (kW, kVA, kVAR, Avg kW, PF)
        //   - Demand (Current, Max Current 24H, kW, Max kW 24H)
        //   - MCCB Status and Trip
        //
        // Total: 18 frames sent sequentially (one per CT)
        //
        if(g_powerCalcEnabled && g_cycleCount > 1)
        {
            uint16_t ct;

            // Update statistics for all 18 CTs
            for(ct = 1; ct <= BRANCH_TX_NUM_CTS; ct++)
            {
                float irms = gMetrologyWorkingData.phases[ct - 1].readings.RMSCurrent
                             * g_calibration.currentGain[ct - 1];
                const CT_PowerResult* ctResult;
                float realPower = 0.0f;

                // Get power result based on phase
                uint16_t ctIndex = ct - 1;
                uint16_t phase = ctIndex / POWER_CTS_PER_PHASE;
                uint16_t ctInPhase = ctIndex % POWER_CTS_PER_PHASE;

                if(phase == POWER_PHASE_R)
                {
                    ctResult = &g_power3PhaseResults.rPhase.ct[ctInPhase];
                }
                else if(phase == POWER_PHASE_Y)
                {
                    ctResult = &g_power3PhaseResults.yPhase.ct[ctInPhase];
                }
                else
                {
                    ctResult = &g_power3PhaseResults.bPhase.ct[ctInPhase];
                }
                realPower = ctResult->realPower_watts;

                // Update branch statistics (min/max/demand tracking)
                BranchTx_updateStats(ct, irms, realPower);
            }

            // Send all 18 branch data frames to S-Board
            // Each frame: 46 bytes, DLC=12 (48 bytes in CAN-FD)
            BranchTx_sendAllBranches();
        }

        //
        //
        // Loop back to wait for sync and capture new samples
        //
    }
}
#endif /* main_FULL — disabled while bringing up BU FW receiver */

//
// initADC_DMA - Configure ADC INT2 and DMA for proper triggering
// Note: ADC INT1 remains enabled for DAC output in adcA1ISR
//
void initADC_DMA(void)
{
    //
    // Configure ADC INT2 for DMA triggering
    // INT2 triggers on EOC0 (same as INT1)
    // Continuous mode auto-clears the flag - THIS IS THE KEY!
    //
    ADC_setInterruptSource(myADC0_BASE, ADC_INT_NUMBER2, ADC_INT_TRIGGER_EOC0);
    ADC_enableContinuousMode(myADC0_BASE, ADC_INT_NUMBER2);
    ADC_enableInterrupt(myADC0_BASE, ADC_INT_NUMBER2);
    ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER2);

    //
    // Configure DMA addresses
    // Source: ADC Result Register 0
    // Destination: Results buffer in RAM
    //
    DMA_configAddresses(myDMA0_BASE,
                        (uint16_t *)myADC0Results,
                        (uint16_t *)(ADCARESULT_BASE + ADC_O_RESULT0));

    //
    // Configure DMA burst and transfer
    // Burst: 1 word per trigger
    // Transfer: RESULTS_BUFFER_SIZE total transfers
    //
    DMA_configBurst(myDMA0_BASE, 1, 0, 1);      // burstSize=1, srcStep=0, destStep=1
    DMA_configTransfer(myDMA0_BASE, RESULTS_BUFFER_SIZE, 0, 1);

    //
    // Configure DMA mode
    // Trigger: ADC INT2 (not INT1!)
    // One-shot: Disabled
    // Continuous: Disabled (stop after transferSize transfers)
    // Size: 16-bit transfers
    //
    DMA_configMode(myDMA0_BASE, DMA_TRIGGER_ADCA2,
                   DMA_CFG_ONESHOT_DISABLE |
                   DMA_CFG_CONTINUOUS_DISABLE |
                   DMA_CFG_SIZE_16BIT);

    //
    // Configure DMA interrupt at end of transfer
    //
    DMA_setInterruptMode(myDMA0_BASE, DMA_INT_AT_END);
    DMA_enableInterrupt(myDMA0_BASE);

    //
    // Enable DMA trigger
    //
    DMA_enableTrigger(myDMA0_BASE);
}

//
// initEPWM - Function to configure ePWM7 to generate the SOC
//
void initEPWM(void)
{
    //
    // Disable SOCA initially
    //
    EPWM_disableADCTrigger(EPWM7_BASE, EPWM_SOC_A);

    //
    // Configure the SOC to occur on up-count when TBCTR = CMPA
    //
    EPWM_setADCTriggerSource(EPWM7_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_U_CMPA);
    EPWM_setADCTriggerEventPrescale(EPWM7_BASE, EPWM_SOC_A, 1);

    //
    // Set the compare A value and period
    // With SYSCLK = 120MHz and period = 23437-1:
    // PWM frequency = 120MHz / 23437 ≈ 5.12kHz
    // Sample rate = 5.12kHz
    //
    EPWM_setCounterCompareValue(EPWM7_BASE, EPWM_COUNTER_COMPARE_A, 11719);
    EPWM_setTimeBasePeriod(EPWM7_BASE, 23437 - 1);

    //
    // Set the local ePWM module clock divider to /1
    //
    EPWM_setClockPrescaler(EPWM7_BASE,
                           EPWM_CLOCK_DIVIDER_1,
                           EPWM_HSCLOCK_DIVIDER_1);

    //
    // Configure Action Qualifier for EPWM7B (GPIO29)
    // HIGH on counter zero, LOW when counter reaches CMPA
    // This produces a PWM output in sync with GPIO28 sync pulse
    //
    EPWM_setActionQualifierAction(EPWM7_BASE, EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);
    EPWM_setActionQualifierAction(EPWM7_BASE, EPWM_AQ_OUTPUT_B,
                                  EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);

    //
    // Disable Trip Zone action on EPWM7B so TZ events don't force it HIGH
    // SysConfig sets TZB action = HIGH which latches GPIO29 HIGH on any trip
    //
    EPWM_setTripZoneAction(EPWM7_BASE, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_DISABLE);

    //
    // Clear any pending Trip Zone flags
    //
    EPWM_clearTripZoneFlag(EPWM7_BASE, EPWM_TZ_FLAG_OST | EPWM_TZ_FLAG_CBC);

    //
    // Enable SOCA event counter initialization
    //
    EPWM_enableADCTriggerEventCountInit(EPWM7_BASE, EPWM_SOC_A);

    //
    // Freeze the counter initially
    //
    EPWM_setTimeBaseCounterMode(EPWM7_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
}

//
// dmaCh1ISR - DMA Channel 1 ISR
// Called when ADC A DMA transfer (15 CT channels) is complete
//
#pragma CODE_SECTION(dmaCh1ISR, ".TI.ramfunc");
__interrupt void dmaCh1ISR(void)
{
    //
    // Call CT DMA handler for ADC A
    //
    CT_DMA_ch1ISR();

    //
    // Set legacy done flag when all CT DMA complete
    //
    if(CT_DMA_isComplete())
    {
        done = 1;
    }

    //
    // Acknowledge interrupt to allow future interrupts from Group 7
    //
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP7);
}

//
// dmaCh2ISR - DMA Channel 2 ISR
// Called when ADC E DMA transfer (3 CT channels) is complete
//
#pragma CODE_SECTION(dmaCh2ISR, ".TI.ramfunc");
__interrupt void dmaCh2ISR(void)
{
    //
    // Call CT DMA handler for ADC E
    //
    CT_DMA_ch2ISR();

    //
    // Set legacy done flag when all CT DMA complete
    //
    if(CT_DMA_isComplete())
    {
        done = 1;
    }

    //
    // Acknowledge interrupt to allow future interrupts from Group 7
    //
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP7);
}

//
// adcA1ISR - ADC A Interrupt 1 ISR
// During playback mode: outputs buffer samples to DAC at 5.12kHz rate
// During capture mode: does nothing (DMA handles the data)
//
#pragma CODE_SECTION(adcA1ISR, ".TI.ramfunc");
__interrupt void adcA1ISR(void)
{
        ADCResults[0]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0) - ADC_MIDPOINT;
        ADCResults[1]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1) - ADC_MIDPOINT;
        ADCResults[2]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2) - ADC_MIDPOINT;
        ADCResults[3]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER3) - ADC_MIDPOINT;
        ADCResults[4]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER4) - ADC_MIDPOINT;
        ADCResults[5]  = (int32_t)ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER0) - ADC_MIDPOINT;
        ADCResults[6]  = (int32_t)ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER1) - ADC_MIDPOINT;
        ADCResults[7]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER5) - ADC_MIDPOINT;
        ADCResults[8]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER6) - ADC_MIDPOINT;
        ADCResults[9]  = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER7) - ADC_MIDPOINT;
        ADCResults[10] = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER8) - ADC_MIDPOINT;
        ADCResults[11] = (int32_t)ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER2) - ADC_MIDPOINT;
        ADCResults[12] = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER9) - ADC_MIDPOINT;
        ADCResults[13] = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER10) - ADC_MIDPOINT;
        ADCResults[14] = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER11) - ADC_MIDPOINT;
        ADCResults[15] = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER12) - ADC_MIDPOINT;
        ADCResults[16] = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER13) - ADC_MIDPOINT;
        ADCResults[17] = (int32_t)ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER14) - ADC_MIDPOINT;
    //
    // Check if we are in playback mode
    //
    if(isPlaybackMode)
    {
        //
        // Output CT1 current waveform to DAC (from inactive ping-pong buffer)
        //
        {
            volatile int16_t* ct1Buf = CT_DMA_getInactiveCTBuffer(1);
            dacOutput = (uint16_t)((int32_t)ct1Buf[playbackIndex] + 2048);
        }
        DAC_setShadowValue(DAC_BASE, dacOutput);

        //
        // Advance to next sample
        //
        playbackIndex++;

        //
        // Check if playback is complete
        //
        if(playbackIndex >= RESULTS_BUFFER_SIZE)
        {
            done = 1;  // Signal playback complete
            playbackIndex = 0;
        }
    }
    if (gAppHandle.adcStatus == ADC_READY) {
        get_debugData(&gMetrologyWorkingData);
        Metrology_perSampleProcessing(&gMetrologyWorkingData);
    }

    //THD
    uint16_t res0 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
    uint16_t res1 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);
    uint16_t res2 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2);
    uint16_t res3 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER3);
    uint16_t res4 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER4);
    uint16_t res5 = ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER0);
    uint16_t res6 = ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER1);
    uint16_t res7 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER5);
    uint16_t res8 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER6);
    uint16_t res9 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER7);
    uint16_t res10 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER8);
    uint16_t res11 = ADC_readResult(ADCERESULT_BASE, ADC_SOC_NUMBER2);
    uint16_t res12 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER9);
    uint16_t res13 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER10);
    uint16_t res14 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER11);
    uint16_t res15 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER12);
    uint16_t res16 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER13);
    uint16_t res17 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER14);




    g_adcState.conversionCount++;

    if(g_adcState.sampleIndex < THD_FFT_SIZE)
    {
        uint16_t idx = g_adcState.sampleIndex;

        g_adcState.rawBuffers[0 ][idx] = (float32_t)res0  * THD_ADC_SCALE;
        g_adcState.rawBuffers[1 ][idx] = (float32_t)res1  * THD_ADC_SCALE;
        g_adcState.rawBuffers[2 ][idx] = (float32_t)res2  * THD_ADC_SCALE;
        g_adcState.rawBuffers[3 ][idx] = (float32_t)res3  * THD_ADC_SCALE;
        g_adcState.rawBuffers[4 ][idx] = (float32_t)res4  * THD_ADC_SCALE;
        g_adcState.rawBuffers[5 ][idx] = (float32_t)res5  * THD_ADC_SCALE;
        g_adcState.rawBuffers[6 ][idx] = (float32_t)res6  * THD_ADC_SCALE;
        g_adcState.rawBuffers[7 ][idx] = (float32_t)res7  * THD_ADC_SCALE;
        g_adcState.rawBuffers[8 ][idx] = (float32_t)res8  * THD_ADC_SCALE;
        g_adcState.rawBuffers[9 ][idx] = (float32_t)res9  * THD_ADC_SCALE;
        g_adcState.rawBuffers[10][idx] = (float32_t)res10 * THD_ADC_SCALE;
        g_adcState.rawBuffers[11][idx] = (float32_t)res11 * THD_ADC_SCALE;
        g_adcState.rawBuffers[12][idx] = (float32_t)res12 * THD_ADC_SCALE;
        g_adcState.rawBuffers[13][idx] = (float32_t)res13 * THD_ADC_SCALE;
        g_adcState.rawBuffers[14][idx] = (float32_t)res14 * THD_ADC_SCALE;
        g_adcState.rawBuffers[15][idx] = (float32_t)res15 * THD_ADC_SCALE;
        g_adcState.rawBuffers[16][idx] = (float32_t)res16 * THD_ADC_SCALE;
        g_adcState.rawBuffers[17][idx] = (float32_t)res17 * THD_ADC_SCALE;

        g_adcState.sampleIndex++;
    }

    if(g_adcState.sampleIndex >= THD_FFT_SIZE)
    {
        g_adcState.bufferFull = 1;
     //   Interrupt_disable(INT_ADCA1);
    }

    //
    // Clear the interrupt flag
    //
    ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);

    //
    // Check if overflow has occurred
    //
    if(true == ADC_getInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1))
    {
        ADC_clearInterruptOverflowStatus(myADC0_BASE, ADC_INT_NUMBER1);
    }

    //
    // Acknowledge the interrupt
    //
    Interrupt_clearACKGroup(INT_myADC0_1_INTERRUPT_ACK_GROUP);
}

//
// cpuTimer0ISR - CPU Timer 0 ISR for sync loss detection
//
__interrupt void cpuTimer0ISR(void)
{
    cpuTimer0IntCount++;

    /* 5-second heartbeat: cpuTimer0 fires at ~6.4 kHz (0.156ms period)
     * 5s / 0.000156s = ~32000 ticks */
    {
        static uint32_t hbCount = 0;
        if (++hbCount >= 32000U)
        {
            hbCount = 0;
            sendAppHeartbeat();
        }
    }

    if (firstpulse)
    {
        syncLossCounter++;

        if (syncLossCounter > 2)
        {
            EPWM_setTimeBaseCounterMode(myEPWM7_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
            firstpulse = false;
            syncLossCounter = 0;
        }
    }

#ifdef CAN_ENABLED
    //
    // Calibration phase only - discovery/calibration retries
    // Does not run during normal runtime
    //
    if (g_calibrationPhase)
    {
        static uint16_t retrySubCount = 0;
        retrySubCount++;

        if (retrySubCount >= 3200U)   // ~500ms at 0.156ms/tick
        {
            retrySubCount = 0;

            //
            // Discovery response: send up to 2 more times, stop on ACK
            //
            if (g_discoveryRetryCount > 0U && !g_discoveryAckRxd)
            {
                CAN_sendDiscoveryResponse();
                g_discoveryRetryCount--;
            }

            //
            // Calibration TX: send up to 4 times until ACK received
            //
            if (g_calibTxRetryCount > 0U && !g_calTxAckRxd)
            {
                CAN_sendCalibrationGains();
                g_calibTxRetryCount--;
            }
        }
    }
#endif

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

//
// GPIOISR - GPIO Interrupt for sync pulse detection
//
__interrupt void GPIOISR(void)
{
    // Start EPWM7 on first GPIO28 sync pulse
    // EPWM7B output on GPIO29 will be in sync with GPIO28
    if (!firstpulse)
    {
        EPWM_setTimeBaseCounter(EPWM7_BASE, 0);  // Reset counter to 0 for clean start
        EPWM_setTimeBaseCounterMode(EPWM7_BASE, EPWM_COUNTER_MODE_UP);
        firstpulse = true;
    }
    syncLossCounter = 0;

    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}

//
// initDAC - Initialize DAC for analog output
//
void initDAC(void)
{
    //
    // Set DAC reference voltage to VREFHI (3.3V internal)
    //
    DAC_setReferenceVoltage(DAC_BASE, DAC_REF_ADC_VREFHI);

    //
    // Set DAC gain mode to 1x
    //
    DAC_setGainMode(DAC_BASE, DAC_GAIN_ONE);

    //
    // Set DAC load mode - load on next SYSCLK
    //
    DAC_setLoadMode(DAC_BASE, DAC_LOAD_SYSCLK);

    //
    // Enable DAC output
    //
    DAC_enableOutput(DAC_BASE);

    //
    // Set initial DAC output to midscale (2048 for 12-bit DAC)
    //
    DAC_setShadowValue(DAC_BASE, 2048);

    //
    // Delay for DAC to power up
    //
    DEVICE_DELAY_US(10);
}

//
// initMCAN - Initialize MCAN for CAN-FD voltage data reception
// Full custom init: MCANConfig() sets up FD mode, bit timing, message RAM, filters (IDs 5,6,7 -> FIFO 0)
// MCANIntrConfig() enables FIFO/buffer interrupts on Line 1 and registers ISR
//
void initMCAN(void)
{
#ifdef CAN_ENABLED
    //
    // Initialize DIP switches for board address configuration
    //
    initDIPSwitchGPIO();

    //
    // Read board address from DIP switches
    //
    readCANAddress();

    //
    // Full MCAN initialization: FD mode, bit timing, message RAM, filters
    // Filters: ID 5, 6, 7 -> RX FIFO 0 + reject-all
    //
    MCANConfig();

    //
    // Configure MCAN interrupts: FIFO 0 + DRX on Line 1, register ISR
    //
    MCANIntrConfig();

    //
    // Initialize voltage reception module
    //
    VoltageRx_init(CAN_BU_BASE);
#endif
}

//
// initCT_DMA - Initialize 18-channel CT DMA module
//
// Configures DMA CH1 for ADC A (15 channels) and DMA CH2 for ADC E (3 channels)
// Each channel captures TOTAL_SAMPLE_COUNT samples (configurable via CYCLES_TO_CAPTURE)
//
// CT Channel Assignment:
//   R-Phase: CT1-CT5 (ADC A SOC0-4), CT6 (ADC E SOC0)
//   Y-Phase: CT7 (ADC E SOC1), CT8-CT11 (ADC A SOC5-8), CT12 (ADC E SOC2)
//   B-Phase: CT13-CT18 (ADC A SOC9-14)
//
void initCT_DMA(void)
{
    //
    // Initialize the CT DMA module
    // This configures:
    //   - ADC A INT2 to trigger on EOC14 (last SOC)
    //   - ADC E INT2 to trigger on EOC2 (last SOC)
    //   - DMA CH1 for ADC A (15 channels, burst of 15 words)
    //   - DMA CH2 for ADC E (3 channels, burst of 3 words)
    //
    CT_DMA_init();
}

//
// End of file
//
