/**
 * @file pdu_manager.h
 * @brief PDU data structures, calibration management, and flash persistence
 *
 * Data flow overview:
 *   M-Board  --[CAN-FD 15 frames]--> receivedBuffer (wire format)
 *            --[RxBufferToStruct]--> pduManager.readWriteData (struct layout)
 *            --[flash write]-------> Bank 4 sector (struct layout + CRC)
 *            --[syncReadWriteToWorking]--> pduManager.workingData (runtime use)
 *
 * IMPORTANT: The wire format (from M-Board) and struct layout differ in the
 * 19 S-board parameter region.  Wire format interleaves primary/secondary;
 * struct groups all primary together, then all secondary.  Explicit field
 * assignments in RxBufferToStruct / structToWireBuffer handle the mapping.
 */

#ifndef PDU_MANAGER_H
#define PDU_MANAGER_H

#include "common.h"

/* ============================================================
 * Wire-format buffer layout (receivedBuffer from M-Board)
 * ============================================================
 * Index range                         Content
 * -----------------------------------------------------------------
 * [0 .. numberOfCTs-1]                ctGain[0..215]        (216 words)
 * [numberOfCTs .. 2*numberOfCTs-1]    kwGain[0..215]        (216 words)
 * [SBOARD_PARAMS_OFFSET + 0..18]     19 S-board parameters ( 19 words)
 * [WIRE_CRC_OFFSET]                  CRC16                 (  1 word)
 * -----------------------------------------------------------------
 * Total payload (excl. CRC): WIRE_DATA_SIZE = 451 words
 * ============================================================ */

/* Base offset where S-board parameters begin in the wire buffer */
#define SBOARD_PARAMS_OFFSET    (2 * numberOfCTs)  /* 432 */
#define NUM_SBOARD_PARAMS       19
#define WIRE_DATA_SIZE          (SBOARD_PARAMS_OFFSET + NUM_SBOARD_PARAMS) /* 451 */
#define WIRE_CRC_OFFSET         WIRE_DATA_SIZE     /* 451 */

/* Absolute wire-buffer indices for the 19 S-board calibration parameters.
 * Order matches exactly what M-Board sends (interleaved pri/sec). */
#define WIRE_PRI_VOLT_R         (SBOARD_PARAMS_OFFSET +  0)  /* 432 */
#define WIRE_PRI_VOLT_Y         (SBOARD_PARAMS_OFFSET +  1)  /* 433 */
#define WIRE_PRI_VOLT_B         (SBOARD_PARAMS_OFFSET +  2)  /* 434 */
#define WIRE_SEC_VOLT_R         (SBOARD_PARAMS_OFFSET +  3)  /* 435 */
#define WIRE_SEC_VOLT_Y         (SBOARD_PARAMS_OFFSET +  4)  /* 436 */
#define WIRE_SEC_VOLT_B         (SBOARD_PARAMS_OFFSET +  5)  /* 437 */
#define WIRE_PRI_CURR_R         (SBOARD_PARAMS_OFFSET +  6)  /* 438 */
#define WIRE_PRI_CURR_Y         (SBOARD_PARAMS_OFFSET +  7)  /* 439 */
#define WIRE_PRI_CURR_B         (SBOARD_PARAMS_OFFSET +  8)  /* 440 */
#define WIRE_SEC_CURR_R         (SBOARD_PARAMS_OFFSET +  9)  /* 441 */
#define WIRE_SEC_CURR_Y         (SBOARD_PARAMS_OFFSET + 10)  /* 442 */
#define WIRE_SEC_CURR_B         (SBOARD_PARAMS_OFFSET + 11)  /* 443 */
#define WIRE_PRI_NEUTRAL        (SBOARD_PARAMS_OFFSET + 12)  /* 444 */
#define WIRE_SEC_NEUTRAL        (SBOARD_PARAMS_OFFSET + 13)  /* 445 */
#define WIRE_PRI_TOTAL_KW       (SBOARD_PARAMS_OFFSET + 14)  /* 446 */
#define WIRE_SEC_KW_R           (SBOARD_PARAMS_OFFSET + 15)  /* 447 */
#define WIRE_SEC_KW_Y           (SBOARD_PARAMS_OFFSET + 16)  /* 448 */
#define WIRE_SEC_KW_B           (SBOARD_PARAMS_OFFSET + 17)  /* 449 */
#define WIRE_GROUND_CURR        (SBOARD_PARAMS_OFFSET + 18)  /* 450 */

/* Gain validation range (value / 10000 = actual multiplier) */
#define GAIN_MIN                1
#define GAIN_MAX                50000
#define GAIN_DEFAULT            10000  /* 1.0x multiplier */

/* ============================================================
 * Data Structures
 *
 * IMPORTANT: Do NOT reorder fields — the struct layout is stored
 * directly in flash and read back via memcpy.  Any change breaks
 * backwards compatibility with existing flash sectors.
 * ============================================================ */

/* Three-phase measurement triplet */
typedef struct {
    uint16_t R;
    uint16_t Y;
    uint16_t B;
} ThreePhaseValues;

/* Primary side measurements (8 words) */
typedef struct {
    ThreePhaseValues voltage;       /* 3 words */
    ThreePhaseValues current;       /* 3 words */
    uint16_t neutralCurrent;        /* 1 word  */
    uint16_t totalKW;               /* 1 word  */
} PrimaryMeasurements;

/* Secondary side measurements (10 words) */
typedef struct {
    ThreePhaseValues voltage;       /* 3 words */
    ThreePhaseValues current;       /* 3 words */
    uint16_t neutralCurrent;        /* 1 word  */
    ThreePhaseValues kw;            /* 3 words */
} SecondaryMeasurements;

/* BU-board calibration gains (432 words) */
typedef struct {
    uint16_t ctGain[numberOfCTs];   /* 216 words — CT current gains */
    uint16_t kwGain[numberOfCTs];   /* 216 words — kW power gains  */
} CalibrationGains;

/* Complete PDU measurement + calibration data (451 words)
 * gains(432) + primary(8) + secondary(10) + groundCurrent(1) = 451 */
struct PDUData {
    CalibrationGains gains;
    PrimaryMeasurements primary;
    SecondaryMeasurements secondary;
    uint16_t groundCurrent;
};

/* Flash-storable calibration record (453 words)
 * Written to one Bank 4 sector per update */
struct CalibrationData {
    uint16_t sectorIndex;           /* Auto-incrementing write counter */
    PDUData pduData;                /* 451 words */
    uint16_t crc;                   /* CRC16 over pduData */
};

/* Double-buffered PDU data manager */
struct PDUDataManager {
    CalibrationData readWriteData;  /* For flash read/write operations */
    CalibrationData workingData;    /* For active runtime operations  */
    bool dataSync;                  /* true when both copies match    */
};

/* CAN-FD frame for M-Board transmission */
struct CANFrame {
    uint8_t data[64];
    uint16_t order;                 /* Frame sequence number (1..NUM_FRAMES) */
};

/* ============================================================
 * Function Prototypes
 * ============================================================ */

/* Initialization */
void initializePDUManager(void);

/* Sync between readWriteData and workingData */
void syncWorkingToReadWrite(void);
void syncReadWriteToWorking(void);

/* M-Board receive path:
 *   receivedBuffer (wire format) --> readWriteData (struct) --> Buffer (flash)
 * Returns true on success (CRC valid), false on CRC mismatch. */
bool RxBufferToStruct(void);

/* Write factory-default calibration (all gains = 10000).
 * Populates readWriteData, computes CRC, and prepares Buffer for flash. */
void writeDefaultCalibrationValues(void);

/* Validate all gain fields in readWriteData.pduData.
 * Clamps out-of-range values to GAIN_DEFAULT. */
void validateCalibrationGains(void);

/* CRC operations */
uint16_t calculateCRC(uint16_t *data, size_t length);
void     computeFlashCRC(CalibrationData *calData);
bool     validateWireCRC(void);
bool     validateFlashCRC(const CalibrationData *calData);

/* Flash write preparation: copies readWriteData to Buffer[], sets datasize.
 * Call Example_ProgramUsingAutoECC() after this. */
void prepareForFlashWrite(void);

/* Flash wrap-around: erases Bank 4 and resets counters if newestData >= WRAP_LIMIT.
 * Returns true if bank was erased. */
bool checkFlashWrapAround(void);

/* CAN frame packing for M-Board transmission */
void prepareFramesForTransmission(uint16_t *buffer, uint16_t bufferSize);

/* Convert struct-layout PDUData back to wire-format buffer (for M-Board responses).
 * wireBuffer must be at least WIRE_DATA_SIZE words. */
void structToWireBuffer(const PDUData *pduData, uint16_t *wireBuffer);

/* External variable declarations */
extern PDUDataManager pduManager;
extern CANFrame storedFrames[NUM_FRAMES];
extern uint16_t receivedBuffer[TOTAL_BUFFER_SIZE];

#endif /* PDU_MANAGER_H */
