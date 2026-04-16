/*
 * voltage_rx.h
 *
 * Three-Phase Voltage Reception via CAN
 *
 * Receives R, Y, B phase voltages via CAN ID 5
 * Sample count configured in power_config.h via CYCLES_TO_CAPTURE:
 *   - 2 cycles  = 256 samples/phase
 *   - 5 cycles  = 640 samples/phase
 *   - 10 cycles = 1280 samples/phase
 *
 * Each phase has its own global buffer:
 *   g_RPhaseVoltageBuffer[], g_YPhaseVoltageBuffer[], g_BPhaseVoltageBuffer[]
 */

#ifndef VOLTAGE_RX_H_
#define VOLTAGE_RX_H_

#include "can_bu.h"

#ifdef CAN_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "power_config.h"  /* Central configuration for sample count */
#include "power_calc.h"    /* For ping-pong buffer access */

/* ========================================================================== */
/*                                  Macros                                    */
/* ========================================================================== */

#define VOLTAGE_CAN_ID              (5U)        /* CAN ID for voltage data */

/* VOLTAGE_BUFFER_SIZE, VOLTAGE_VALUES_PER_FRAME, and VOLTAGE_TOTAL_FRAMES
 * are now defined in power_config.h to ensure consistency across all modules */

/* Frame byte positions */
#define FRAME_PHASE_BYTE            (0U)
#define FRAME_NUMBER_BYTE           (1U)
#define FRAME_DATA_START_BYTE       (2U)

/* Phase identifiers in CAN frame byte 0 */
#define PHASE_ID_R                  (1U)
#define PHASE_ID_Y                  (2U)
#define PHASE_ID_B                  (3U)
#define NUM_PHASES                  (3U)

/* S-Board command bytes in CAN frame byte 0 (received on FIFO 0, ID 5) */
#define CMD_DISCOVERY_START         (0x0BU)
#define CMD_DISCOVERY_STOP          (0x0CU)

/* Total CAN frames for all 3 phases */
#define VOLTAGE_TOTAL_FRAMES_ALL_PHASES  (VOLTAGE_TOTAL_FRAMES * NUM_PHASES)

/* ========================================================================== */
/*                             Type Definitions                               */
/* ========================================================================== */

typedef struct
{
    bool     isComplete;
    uint32_t framesReceived;
    uint32_t framesBitmap;
    uint32_t lastError;
} VoltageRx_Status;

/* Three-phase reception status */
typedef struct
{
    bool     allPhasesComplete;     /* True when R, Y, B all received */
    bool     rPhaseComplete;
    bool     yPhaseComplete;
    bool     bPhaseComplete;
    uint32_t rFramesReceived;
    uint32_t yFramesReceived;
    uint32_t bFramesReceived;
} VoltageRx_ThreePhaseStatus;

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

/* Legacy RX FIFO buffer (kept for compatibility, reduced to 1 element) */
extern MCAN_RxBufElement rxFIFOMsg[1];
/* Total frame counter for debug (voltage values are extracted directly in ISR) */
extern volatile uint32_t rxFIFOMsg_count;

/* Three-phase voltage buffers (size based on CYCLES_TO_CAPTURE in power_config.h) */
extern volatile int16_t g_RPhaseVoltageBuffer[VOLTAGE_BUFFER_SIZE];
extern volatile int16_t g_YPhaseVoltageBuffer[VOLTAGE_BUFFER_SIZE];
extern volatile int16_t g_BPhaseVoltageBuffer[VOLTAGE_BUFFER_SIZE];

/* Reception status (legacy - R phase only) */
extern volatile VoltageRx_Status g_RPhaseRxStatus;

/* Three-phase reception status */
extern volatile VoltageRx_ThreePhaseStatus g_threePhaseRxStatus;

/* Flag indicating all frames received and ready for processing (set by ISR, cleared by main) */
extern volatile bool g_framesReady;

/* Calibration phase flag - set on 0x0B, cleared when 0x06 resets CPU */
extern volatile bool g_calibrationPhase;

/* Discovery retry counter - set to 2 when 0x0B received (first send is immediate) */
extern volatile uint16_t g_discoveryRetryCount;

/* Discovery ACK - set when S-board acknowledges discovery response */
extern volatile bool g_discoveryAckRxd;

/* Calibration TX retry counter - set to 4 when 0x0C received */
extern volatile uint16_t g_calibTxRetryCount;

/* Calibration TX ACK - set when S-board acknowledges calibration data */
extern volatile bool g_calTxAckRxd;

/* Flash command flags - set by ISR, processed by main loop */
extern volatile bool g_cmdEraseBank4;       /* 0x09 command: erase Bank 4 */
extern volatile bool g_cmdSaveCalibration;  /* 0x04 command: save current calibration to flash */

/*
 * g_cmdWriteDefaults uses a magic value instead of bool to guard against
 * memory corruption during flash operations (bool treats any non-zero as true).
 * ISR sets to CMD_WRITE_DEFAULTS_KEY, main loop checks for exact match.
 */
#define CMD_WRITE_DEFAULTS_KEY   0x5A01U
#define CMD_WRITE_DEFAULTS_IDLE  0x0000U
extern volatile uint16_t g_cmdWriteDefaults;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/* Initialize voltage reception */
int32_t VoltageRx_init(uint32_t baseAddr);

/* CAN RX interrupt handler */
void VoltageRx_interruptHandler(uint32_t baseAddr);

/* Copy frames from rxFIFOMsg[] to three-phase voltage buffers */
void VoltageRx_copyToBuffer(void);

/**
 * @brief   Copy voltage data to active ping-pong buffer and swap
 *          This is the main function to call after all CAN frames received
 */
void VoltageRx_copyToPingPongBuffer(void);

/* Reset for next reception */
void VoltageRx_reset(void);

/* Check if all voltage values received (TOTAL_SAMPLE_COUNT samples) */
bool VoltageRx_isComplete(void);

/* Check if all three phases are complete */
bool VoltageRx_isThreePhaseComplete(void);

/* Get pointer to voltage buffer (legacy - returns g_RPhaseVoltageBuffer) */
volatile uint16_t* VoltageRx_getBuffer(void);

/* Get pointer to specific phase voltage buffer */
volatile int16_t* VoltageRx_getPhaseBuffer(uint8_t phaseId);

/* Get three-phase status */
VoltageRx_ThreePhaseStatus* VoltageRx_getThreePhaseStatus(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_ENABLED */

#endif /* VOLTAGE_RX_H_ */
