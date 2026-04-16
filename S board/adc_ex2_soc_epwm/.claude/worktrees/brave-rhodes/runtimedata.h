/**
 * @file runtimedata.h
 * @brief Runtime BU board data collection and management
 */

#ifndef RUNTIMEDATA_H
#define RUNTIMEDATA_H

#include "common.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_BU_BOARDS    12
#define CTS_PER_BOARD    18
#define NUM_BRANCHES     (MAX_BU_BOARDS * CTS_PER_BOARD)

// Total frames expected from all BU boards
#define TOTAL_BU_FRAMES  (MAX_BU_BOARDS * CTS_PER_BOARD)

// Frame tracking using 32-bit words (216 bits = 7 words)
#define FRAME_MASK_WORDS 7

#pragma pack(push, 1)

/**
 * @brief Branch Parameters (matches BU Branch Frame)
 */
typedef struct {
    uint16_t branchCurrent;
    uint16_t branchMaxCurrent;
    uint16_t branchMinCurrent;
    uint16_t branchLoadPercentage;        // New field based on CSV
    uint16_t branchCurrentTHD;
    uint16_t branchVoltageTHD;
    uint16_t branchKW;
    uint16_t branchKVA;
    uint16_t branchKVAR;
    uint16_t branchAvgKW;
    uint16_t branchPowerFactor;
    uint16_t branchCurrentDemandHourly;
    uint16_t branchMaxCurrentDemand24Hour;
    uint16_t branchKWDemandHourly;
    uint16_t branchMaxKWDemand24Hour;
    uint8_t  mccbStatus;                  // Byte 34
    uint8_t  mccbTrip;                    // Byte 35

} BranchParameters_t;

/**
 * @brief Input Parameters (matches Input Frames 1 & 2)
 */
typedef struct {
    // --- Frame 1 Data ---
    uint16_t inputGroundCurrent;
    uint16_t inputVoltageRP;        // Line-Neutral
    uint16_t inputVoltageYP;
    uint16_t inputVoltageBP;
    uint16_t inputAverageVoltageLN;
    uint16_t inputVoltageRY;        // Line-Line
    uint16_t inputVoltageYB;
    uint16_t inputVoltageBR;
    uint16_t inputAverageVoltageLL;
    uint16_t inputCurrentRP;
    uint16_t inputCurrentYP;
    uint16_t inputCurrentBP;
    uint16_t inputAverageCurrentLN;
    uint16_t inputCurrentNeutral;
    uint16_t inputCurrentTHDRP;
    uint16_t inputCurrentTHDYP;
    uint16_t inputCurrentTHDBP;
    uint16_t inputMaxCurrentRP;
    uint16_t inputMaxCurrentYP;
    uint16_t inputMaxCurrentBP;
    uint16_t inputMinCurrentRP;
    uint16_t inputMinCurrentYP;
    uint16_t inputMinCurrentBP;
    uint16_t inputMaxVoltageRP;
    uint16_t inputMaxVoltageYP;
    uint16_t inputMaxVoltageBP;
    uint16_t inputMinVoltageRP;
    uint16_t inputMinVoltageYP;
    uint16_t inputMinVoltageBP;
    uint16_t inputFrequency;        // Single frequency field in Frame 1 (Bytes 62-63)

    // --- Frame 2 Data ---
    uint16_t inputVoltageTHDRP;
    uint16_t inputVoltageTHDYP;
    uint16_t inputVoltageTHDBP;
    uint16_t inputCrestFactorRP;
    uint16_t inputCrestFactorYP;
    uint16_t inputCrestFactorBP;

    // Power
    uint16_t inputKWRP;
    uint16_t inputKWYP;
    uint16_t inputKWBP;
    uint16_t inputTotalKW;

    uint16_t inputKVARP;
    uint16_t inputKVAYP;
    uint16_t inputKVABP;
    uint16_t inputTotalKVA;

    uint16_t inputKVARRP;
    uint16_t inputKVARYP;
    uint16_t inputKVARBP;
    uint16_t inputTotalKVAR;

    // Power Factors
    uint16_t inputPFRP;
    uint16_t inputPFYP;
    uint16_t inputPFBP;
    uint16_t inputPFAvg;

    // Errors
    uint16_t voltageLack;
    uint16_t voltageUnbalance;
    uint16_t phaseLoss;
    uint8_t  phaseSequenceError;    // 1 Byte
} InputParameters_t;

/**
 * @brief Output Parameters (matches Output Frames 1 & 2)
 */
typedef struct {
    // --- Frame 1 Data ---
    uint16_t outputVoltageRP;       // Line-Neutral
    uint16_t outputVoltageYP;
    uint16_t outputVoltageBP;
    uint16_t outputAverageVoltageLN;
    uint16_t outputVoltageRY;       // Line-Line
    uint16_t outputVoltageYB;
    uint16_t outputVoltageBR;
    uint16_t outputAverageVoltageLL;
    uint16_t outputCurrentRP;
    uint16_t outputCurrentYP;
    uint16_t outputCurrentBP;
    uint16_t outputAvgCurrent;
    uint16_t outputCurrentNeutral;
    uint16_t outputFrequencyRP;
    uint16_t outputFrequencyYP;
    uint16_t outputFrequencyBP;

    // Min/Max
    uint16_t outputMaxCurrentR;
    uint16_t outputMaxCurrentY;
    uint16_t outputMaxCurrentB;
    uint16_t outputMinCurrentR;
    uint16_t outputMinCurrentY;
    uint16_t outputMinCurrentB;
    uint16_t outputMaxVoltageR;
    uint16_t outputMaxVoltageY;
    uint16_t outputMaxVoltageB;
    uint16_t outputMinVoltageR;
    uint16_t outputMinVoltageY;
    uint16_t outputMinVoltageB;

    // Errors (Frame 1 end)
    uint8_t  voltageLack;
    uint8_t  voltageUnbalance;
    uint8_t  phaseLoss;
    uint8_t  phaseSequenceError;

    // --- Frame 2 Data ---
    uint16_t outputVoltageTHDRP;
    uint16_t outputVoltageTHDYP;
    uint16_t outputVoltageTHDBP;
    uint16_t outputCurrentTHDRP;
    uint16_t outputCurrentTHDYP;
    uint16_t outputCurrentTHDBP;
    uint16_t outputCurrentTHDNeutral;
    uint16_t outputCrestFactorRP;
    uint16_t outputCrestFactorYP;
    uint16_t outputCrestFactorBP;

    // Power
    uint16_t outputKWPhaseR;
    uint16_t outputKWPhaseY;
    uint16_t outputKWPhaseB;
    uint16_t outputTotalKW;

    uint16_t outputKVAPhaseR;
    uint16_t outputKVAPhaseY;
    uint16_t outputKVAPhaseB;
    uint16_t outputTotalKVA;

    uint16_t outputKVARPhaseR;
    uint16_t outputKVARPhaseY;
    uint16_t outputKVARPhaseB;
    uint16_t outputTotalKVAR;

    // Power Factors
    uint16_t outputPFRP;
    uint16_t outputPFYP;
    uint16_t outputPFBP;
    uint16_t outputPFAvg;

} OutputParameters_t;

typedef struct {
    BranchParameters_t branchParams[NUM_BRANCHES];
    InputParameters_t inputParams;
    OutputParameters_t outputParams;
} SBoardData_t;

#pragma pack(pop)

// Runtime data collection functions
void initRuntimeDataCollection(void);
void processBURuntimeFrame(const MCAN_RxBufElement *rxMsg);
void processBURuntimeMessages(void);
bool checkAllBUFramesReceived(void);
void populateInputParameters(void);
void populateOutputParameters(void);
void resetRuntimeDataCollection(void);
void resetRuntimeFrameTracking(void);

// BU board firmware version (one per board, extracted from runtime frames)
extern uint8_t buBoardVersion[MAX_BU_BOARDS];

// External variable declarations
extern SBoardData_t sBoardRuntimeData;
extern uint32_t frameReceivedMask[FRAME_MASK_WORDS];
extern bool allBUFramesReceived;
extern uint16_t totalFramesReceived;
extern bool runtimeMessagePending[MCAN_RX_BUFF_NUM];
extern bool runtimeMessageReceived;

#endif // RUNTIMEDATA_H
