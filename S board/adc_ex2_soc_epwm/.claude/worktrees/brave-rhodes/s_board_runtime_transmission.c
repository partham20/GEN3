/**
 * @file s_board_runtime_transmission.c
 * @brief S-Board runtime data transmission to M-Board implementation
 */

#include "s_board_runtime_transmission.h"
#include "can_driver.h"
#include "common.h"
#include "bu_board.h"
#include "flash_driver.h"

// S-Board firmware version — change this value for each firmware release
const uint8_t SBOARD_VERSION = 3;

// Global variables
SBoardRuntimeTx_t sBoardRuntimeTx;

/**
 * @brief Initialize S-Board runtime transmission to M-Board
 */
void initSBoardRuntimeToMBoard(void)
{
    sBoardRuntimeTx.transmissionCounter = 0;
    sBoardRuntimeTx.transmissionInProgress = false;
    sBoardRuntimeTx.currentFrameIndex = 0;
    sBoardRuntimeTx.allFramesSent = false;
}

/**
 * @brief Process S-Board runtime data transmission to M-Board (call from main loop)
 * Sends S-Board parameter frames + BU branch frames
 */
void processSBoardRuntimeToMBoard(void)
{
    // NOTE: populateInputParameters() and populateOutputParameters() are called
    // by the main loop before this function — no need to call them again here.
    // Calling them twice doubled C2000 stack usage and caused 0xFF corruption.

    // Start transmission sequence
    sBoardRuntimeTx.transmissionInProgress = true;
    sBoardRuntimeTx.allFramesSent = false;

    // Send 4 S-Board specific parameter frames FIRST
    // Note: CT-No (Byte 3) is used as the Frame ID for S-Board data (0, 1, 2, 3)
    sendSBoardInputParamFrame1();       // Frame 1: Input Params (Voltages, Currents, Min/Max)
    // DEVICE_DELAY_US(1000);

    sendSBoardInputParamFrame2();       // Frame 2: Input Params (Power, THD, PF, Errors)
    // DEVICE_DELAY_US(1000);

    sendSBoardOutputParamFrame1();      // Frame 3: Output Params (Voltages, Currents, Freq, Min/Max)
    // DEVICE_DELAY_US(1000);

    sendSBoardOutputParamFrame2();      // Frame 4: Output Params (Power, THD, Crest, PF)
    // DEVICE_DELAY_US(1000);

//     Then send all 216 BU branch frames
//     BU Board numbers: 1-12, CT numbers: 0-17 (18 CTs per board)
    sendSBoardBranchFramesToMBoard();

    sBoardRuntimeTx.transmissionInProgress = false;
    sBoardRuntimeTx.allFramesSent = true;
}

/**
 * @brief Helper to pack a 16-bit value into the frame (Little Endian)
 */
static void pack16(uint8_t *buffer, uint16_t *index, uint16_t value)
{
    buffer[(*index)++] = value & 0xFF;
    buffer[(*index)++] = (value >> 8) & 0xFF;
}

/**
 * @brief Send S-Board Input Parameters Frame 1
 * CSV Mapping: "Input Frame 1"
 * Content: Ground I, Voltages(LN/LL), Currents, I_THD, Max/Min I, Max/Min V, Freq
 */
void sendSBoardInputParamFrame1(void)
{
    uint8_t frameData[64];
    memset(frameData, 0, 64);
    uint16_t idx = 4;  // Start after header

    // Frame header (Bytes 0-3)
    frameData[0] = SBOARD_VERSION;              // Version
    frameData[1] = SBOARD_ID;                   // S-Board ID
    frameData[2] = 0x00;                        // BU Controller ID = 0
    frameData[3] = 0x01;                        // CT-No/Frame ID = 0 for Input Frame 1

    InputParameters_t *p = &sBoardRuntimeData.inputParams;

    // Bytes 4-5: Ground Current
    pack16(frameData, &idx, p->inputGroundCurrent);

    // Bytes 6-11: Input Voltage LN (RP, YP, BP)
    pack16(frameData, &idx, p->inputVoltageRP);
    pack16(frameData, &idx, p->inputVoltageYP);
    pack16(frameData, &idx, p->inputVoltageBP);

    // Bytes 12-13: Input Average Voltage LN
    pack16(frameData, &idx, p->inputAverageVoltageLN);

    // Bytes 14-19: Input Voltage RY, YB, BR (Line-to-Line)
    pack16(frameData, &idx, p->inputVoltageRY);
    pack16(frameData, &idx, p->inputVoltageYB);
    pack16(frameData, &idx, p->inputVoltageBR);

    // Bytes 20-21: Input Average Voltage LL
    pack16(frameData, &idx, p->inputAverageVoltageLL);

    // Bytes 22-27: Input Current (RP, YP, BP)
    pack16(frameData, &idx, p->inputCurrentRP);
    pack16(frameData, &idx, p->inputCurrentYP);
    pack16(frameData, &idx, p->inputCurrentBP);

    // Bytes 28-29: Input Average Current LN
    pack16(frameData, &idx, p->inputAverageCurrentLN);

    // Bytes 30-31: Input Current Neutral
    pack16(frameData, &idx, p->inputCurrentNeutral);

    // Bytes 32-37: Input Current THD (RP, YP, BP)
    pack16(frameData, &idx, p->inputCurrentTHDRP);
    pack16(frameData, &idx, p->inputCurrentTHDYP);
    pack16(frameData, &idx, p->inputCurrentTHDBP);

    // Bytes 38-43: Input Max Current (RP, YP, BP)
    pack16(frameData, &idx, p->inputMaxCurrentRP);
    pack16(frameData, &idx, p->inputMaxCurrentYP);
    pack16(frameData, &idx, p->inputMaxCurrentBP);

    // Bytes 44-49: Input Min Current (RP, YP, BP)
    pack16(frameData, &idx, p->inputMinCurrentRP);
    pack16(frameData, &idx, p->inputMinCurrentYP);
    pack16(frameData, &idx, p->inputMinCurrentBP);

    // Bytes 50-55: Input Max Voltage (RP, YP, BP)
    pack16(frameData, &idx, p->inputMaxVoltageRP);
    pack16(frameData, &idx, p->inputMaxVoltageYP);
    pack16(frameData, &idx, p->inputMaxVoltageBP);

    // Bytes 56-61: Input Min Voltage (RP, YP, BP)
    pack16(frameData, &idx, p->inputMinVoltageRP);
    pack16(frameData, &idx, p->inputMinVoltageYP);
    pack16(frameData, &idx, p->inputMinVoltageBP);

    // Bytes 62-63: Input Frequency
    pack16(frameData, &idx, p->inputFrequency);

    sendSingleSBoardRuntimeFrame(frameData);
}

/**
 * @brief Send S-Board Input Parameters Frame 2
 * CSV Mapping: "Input Frame 2"
 * Content: V_THD, Crest Factor, Power (kW, kVA, kVAR), PF, Errors
 */
void sendSBoardInputParamFrame2(void)
{
    uint8_t frameData[64];
    memset(frameData, 0, 64);
    uint16_t idx = 4;

    frameData[0] = SBOARD_VERSION;
    frameData[1] = SBOARD_ID;
    frameData[2] = 0x00;
    frameData[3] = 0x02;                        // CT-No/Frame ID = 1 for Input Frame 2

    InputParameters_t *p = &sBoardRuntimeData.inputParams;

    // Bytes 4-9: Input Voltage THD (RP, YP, BP)
    pack16(frameData, &idx, p->inputVoltageTHDRP); // Note: CSV calls this RP, YP, BP
    pack16(frameData, &idx, p->inputVoltageTHDYP);
    pack16(frameData, &idx, p->inputVoltageTHDBP);

    // Bytes 10-15: Input Crest Factor (RP, YP, BP)
    pack16(frameData, &idx, p->inputCrestFactorRP);
    pack16(frameData, &idx, p->inputCrestFactorYP);
    pack16(frameData, &idx, p->inputCrestFactorBP);

    // Bytes 16-23: Input kW (RP, YP, BP, Total)
    pack16(frameData, &idx, p->inputKWRP);
    pack16(frameData, &idx, p->inputKWYP);
    pack16(frameData, &idx, p->inputKWBP);
    pack16(frameData, &idx, p->inputTotalKW);

    // Bytes 24-31: Input kVA (RP, YP, BP, Total)
    pack16(frameData, &idx, p->inputKVARP);
    pack16(frameData, &idx, p->inputKVAYP);
    pack16(frameData, &idx, p->inputKVABP);
    pack16(frameData, &idx, p->inputTotalKVA);

    // Bytes 32-39: Input kVAR (RP, YP, BP, Total)
    pack16(frameData, &idx, p->inputKVARRP);
    pack16(frameData, &idx, p->inputKVARYP);
    pack16(frameData, &idx, p->inputKVARBP);
    pack16(frameData, &idx, p->inputTotalKVAR);

    // Bytes 40-47: Input PF (RP, YP, BP, Avg)
    pack16(frameData, &idx, p->inputPFRP);
    pack16(frameData, &idx, p->inputPFYP);
    pack16(frameData, &idx, p->inputPFBP);
    pack16(frameData, &idx, p->inputPFAvg);

    // Bytes 48-49: Voltage Lack
    frameData[idx++] = p->voltageLack & 0xFF;

    // Bytes 50-51: Voltage Unbalance
    frameData[idx++] = p->voltageUnbalance & 0xFF;

    // Bytes 52-53: Phase loss
    frameData[idx++] = p->phaseLoss & 0xFF;

    // Byte 54: Phase sequence error (1 Byte)
    frameData[idx++] = p->phaseSequenceError & 0xFF;

    // Bytes 55-63: Reserved

    sendSingleSBoardRuntimeFrame(frameData);
}

/**
 * @brief Send S-Board Output Parameters Frame 1
 * CSV Mapping: "Output Frame 1"
 * Content: Voltages, Currents, Avg, Frequencies, Max/Min I/V, Errors
 */
void sendSBoardOutputParamFrame1(void)
{
    uint8_t frameData[64];
    memset(frameData, 0, 64);
    uint16_t idx = 4;

    frameData[0] = SBOARD_VERSION;
    frameData[1] = SBOARD_ID;
    frameData[2] = 0x00;
    frameData[3] = 0x03;                        // CT-No/Frame ID = 2 for Output Frame 1

    OutputParameters_t *p = &sBoardRuntimeData.outputParams;

    // Bytes 4-9: Output Voltage LN (RP, YP, BP)
    pack16(frameData, &idx, p->outputVoltageRP);
    pack16(frameData, &idx, p->outputVoltageYP);
    pack16(frameData, &idx, p->outputVoltageBP);

    // Bytes 10-11: Output Average Voltage LN
    pack16(frameData, &idx, p->outputAverageVoltageLN);

    // Bytes 12-17: Output Voltage LL (RY, YB, BR)
    pack16(frameData, &idx, p->outputVoltageRY);
    pack16(frameData, &idx, p->outputVoltageYB);
    pack16(frameData, &idx, p->outputVoltageBR);

    // Bytes 18-19: Output Average Voltage LL
    pack16(frameData, &idx, p->outputAverageVoltageLL);

    // Bytes 20-25: Output Current (RP, YP, BP)
    pack16(frameData, &idx, p->outputCurrentRP);
    pack16(frameData, &idx, p->outputCurrentYP);
    pack16(frameData, &idx, p->outputCurrentBP);

    // Bytes 26-27: Output Avg Current
    pack16(frameData, &idx, p->outputAvgCurrent);

    // Bytes 28-29: Output Current Neutral
    pack16(frameData, &idx, p->outputCurrentNeutral);

    // Bytes 30-35: Output Frequency (RP, YP, BP)
    pack16(frameData, &idx, p->outputFrequencyRP);
    pack16(frameData, &idx, p->outputFrequencyYP);
    pack16(frameData, &idx, p->outputFrequencyBP);

    // Bytes 36-41: Output Max Current (R, Y, B)
    pack16(frameData, &idx, p->outputMaxCurrentR);
    pack16(frameData, &idx, p->outputMaxCurrentY);
    pack16(frameData, &idx, p->outputMaxCurrentB);

    // Bytes 42-47: Output Min Current (R, Y, B)
    pack16(frameData, &idx, p->outputMinCurrentR);
    pack16(frameData, &idx, p->outputMinCurrentY);
    pack16(frameData, &idx, p->outputMinCurrentB);

    // Bytes 48-53: Output Max Voltage (R, Y, B)
    pack16(frameData, &idx, p->outputMaxVoltageR);
    pack16(frameData, &idx, p->outputMaxVoltageY);
    pack16(frameData, &idx, p->outputMaxVoltageB);

    // Bytes 54-59: Output Min Voltage (R, Y, B)
    pack16(frameData, &idx, p->outputMinVoltageR);
    pack16(frameData, &idx, p->outputMinVoltageY);
    pack16(frameData, &idx, p->outputMinVoltageB);

    // Byte 60: Voltage Lack
    frameData[idx++] = p->voltageLack & 0xFF;

    // Byte 61: Voltage Unbalance
    frameData[idx++] = p->voltageUnbalance & 0xFF;

    // Byte 62: Phase loss
    frameData[idx++] = p->phaseLoss & 0xFF;

    // Byte 63: Phase sequence error
    frameData[idx++] = p->phaseSequenceError & 0xFF;

    sendSingleSBoardRuntimeFrame(frameData);
}

/**
 * @brief Send S-Board Output Parameters Frame 2
 * CSV Mapping: "Output Frame 2"
 * Content: THD, Crest, Power, PF
 */
void sendSBoardOutputParamFrame2(void)
{
    uint8_t frameData[64];
    memset(frameData, 0, 64);
    uint16_t idx = 4;

    frameData[0] = SBOARD_VERSION;
    frameData[1] = SBOARD_ID;
    frameData[2] = 0x00;
    frameData[3] = 0x04;                        // CT-No/Frame ID = 3 for Output Frame 2

    OutputParameters_t *p = &sBoardRuntimeData.outputParams;

    // Bytes 4-9: Output Voltage THD (RP, YP, BP)
    pack16(frameData, &idx, p->outputVoltageTHDRP);
    pack16(frameData, &idx, p->outputVoltageTHDYP);
    pack16(frameData, &idx, p->outputVoltageTHDBP);

    // Bytes 10-15: Output Current THD (RP, YP, BP)
    pack16(frameData, &idx, p->outputCurrentTHDRP);
    pack16(frameData, &idx, p->outputCurrentTHDYP);
    pack16(frameData, &idx, p->outputCurrentTHDBP);

    // Bytes 16-17: Output Current THD Neutral
    pack16(frameData, &idx, p->outputCurrentTHDNeutral);

    // Bytes 18-23: Output Crest Factor (RP, YP, BP)
    pack16(frameData, &idx, p->outputCrestFactorRP);
    pack16(frameData, &idx, p->outputCrestFactorYP);
    pack16(frameData, &idx, p->outputCrestFactorBP);

    // Bytes 24-31: Output kW (Phase R, Y, B, Total)
    pack16(frameData, &idx, p->outputKWPhaseR);
    pack16(frameData, &idx, p->outputKWPhaseY);
    pack16(frameData, &idx, p->outputKWPhaseB);
    pack16(frameData, &idx, p->outputTotalKW);

    // Bytes 32-39: Output kVA (Phase R, Y, B, Total)
    pack16(frameData, &idx, p->outputKVAPhaseR);
    pack16(frameData, &idx, p->outputKVAPhaseY);
    pack16(frameData, &idx, p->outputKVAPhaseB);
    pack16(frameData, &idx, p->outputTotalKVA);

    // Bytes 40-47: Output kVAR (Phase R, Y, B, Total)
    pack16(frameData, &idx, p->outputKVARPhaseR);
    pack16(frameData, &idx, p->outputKVARPhaseY);
    pack16(frameData, &idx, p->outputKVARPhaseB);
    pack16(frameData, &idx, p->outputTotalKVAR);

    // Bytes 48-55: Output PF (RP, YP, BP, Avg)
    pack16(frameData, &idx, p->outputPFRP);
    pack16(frameData, &idx, p->outputPFYP);
    pack16(frameData, &idx, p->outputPFBP);
    pack16(frameData, &idx, p->outputPFAvg);

    // Bytes 56-63: Reserved

    sendSingleSBoardRuntimeFrame(frameData);
}

/** 0x0F
 * @brief Send all BU branch frames (216 frames) to M-Board
 * BU Board numbers: 1-12, CT numbers: 0-17 (Frames 5-220)
 */
void sendSBoardBranchFramesToMBoard(void)
{
    int branchIndex;
    uint8_t frameData[64];
    uint16_t idx;

    // Send all 216 branch parameter frames (12 BU boards � 18 CTs each)
    for (branchIndex = 0; branchIndex < NUM_BRANCHES; branchIndex++)
    {
        // Calculate BU board and CT numbers
        uint8_t buBoard = (branchIndex / CTS_PER_BOARD) + 1;    // 1-12
        uint8_t ctNumber = branchIndex % CTS_PER_BOARD + 1;     // 1-18 (CSV says "CT-No" column 1..18 usually, or 0..17. Adapting to 1-based for CAN ID usually)
        // If your system uses 0-17 for CT index in CAN frame, change `+ 1` to `+ 0`.
        // CSV Header implies indices. Typically S-Board communicates physical indices.

        memset(frameData, 0, 64);
        idx = 4;

        // Frame header (Bytes 0-3)
        frameData[0] = buBoardVersion[buBoard - 1];  // BU board firmware version
        frameData[1] = SBOARD_ID;               // S-Board ID
        frameData[2] = buBoard;                 // BU Board number (1-12)
        frameData[3] = ctNumber;                // CT number

        BranchParameters_t *p = &sBoardRuntimeData.branchParams[branchIndex];

        // Bytes 4-5: Branch Current
        pack16(frameData, &idx, p->branchCurrent);
        // Bytes 6-7: Max Current
        pack16(frameData, &idx, p->branchMaxCurrent);
        // Bytes 8-9: Min Current
        pack16(frameData, &idx, p->branchMinCurrent);

        // Bytes 10-11: Load Percentage
        // If branchLoadPercentage is not in struct, calculate it.
        // CSV says "Branch_LoadPercentage", implying it is a parameter.
        pack16(frameData, &idx, p->branchLoadPercentage);

        // Bytes 12-13: Current THD
        pack16(frameData, &idx, p->branchCurrentTHD);
        // Bytes 14-15: Voltage THD
        pack16(frameData, &idx, p->branchVoltageTHD);
        // Bytes 16-17: KW
        pack16(frameData, &idx, p->branchKW);
        // Bytes 18-19: KVA
        pack16(frameData, &idx, p->branchKVA);
        // Bytes 20-21: KVAR
        pack16(frameData, &idx, p->branchKVAR);
        // Bytes 22-23: Avg KW
        pack16(frameData, &idx, p->branchAvgKW);
        // Bytes 24-25: Power Factor
        pack16(frameData, &idx, p->branchPowerFactor);
        // Bytes 26-27: Current Demand Hourly
        pack16(frameData, &idx, p->branchCurrentDemandHourly);
        // Bytes 28-29: Max Current Demand 24Hour
        pack16(frameData, &idx, p->branchMaxCurrentDemand24Hour);
        // Bytes 30-31: KW Demand Hourly
        pack16(frameData, &idx, p->branchKWDemandHourly);
        // Bytes 32-33: Max KW Demand 24Hour
        pack16(frameData, &idx, p->branchMaxKWDemand24Hour);

        // Byte 34: Branch MCCB Status
        // Assuming struct has distinct fields or status is split
        frameData[idx++] = p->mccbStatus & 0xFF;

        // Byte 35: Branch MCCB Trip
        frameData[idx++] = p->mccbTrip & 0xFF;

        // Bytes 36-63: Reserved

        sendSingleSBoardRuntimeFrame(frameData);

        // DEVICE_DELAY_US(500);
    }
}

/**
 * @brief Send single S-Board runtime frame to M-Board
 * @param frameData Pointer to 64-byte frame data
 */
void sendSingleSBoardRuntimeFrame(const uint8_t* frameData)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    // Configure message properties for M-Board communication
    TxMsg.id = ((uint32_t)(SBOARD_RUNTIME_CAN_ID)) << 18U;
    TxMsg.brs = 0x1;    // Enable bit rate switching
    TxMsg.dlc = 15;     // 64 byte message
    TxMsg.rtr = 0;      // Not a remote frame
    TxMsg.xtd = 0;      // Standard ID
    TxMsg.fdf = 0x1;    // FD format
    TxMsg.esi = 0U;     // Error state indicator
    TxMsg.efc = 1U;     // Event FIFO control
    TxMsg.mm = 0xAAU;   // Message marker

    // Copy frame data to message
    memcpy(TxMsg.data, frameData, 64);

    // Send message via CAN B to M-Board
    MCAN_writeMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(MCANB_DRIVER_BASE, 0U);

    // Wait for transmission to complete (with timeout)
    canTxWaitComplete(MCANB_DRIVER_BASE);
}
