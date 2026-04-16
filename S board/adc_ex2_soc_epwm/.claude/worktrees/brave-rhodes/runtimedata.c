/**
 * @file runtimedata.c
 * @brief Runtime BU board data collection and management implementation
 */

#include "runtimedata.h"
#include "can_driver.h"
#include "s_board_adc.h"
#include "metro.h"
#include "pdu_manager.h"
#include "THD_Module/THD_Module.h"

extern THD_Analyzer g_analyzer;

// Global variables
SBoardData_t sBoardRuntimeData;
uint32_t frameReceivedMask[FRAME_MASK_WORDS];
bool allBUFramesReceived = false;
uint16_t totalFramesReceived = 0,wait_for_minmax=0,MinMax_set=0;
bool runtimeMessagePending[MCAN_RX_BUFF_NUM];
bool runtimeMessageReceived = false;
uint8_t buBoardVersion[MAX_BU_BOARDS];

/**
 * @brief Initialize runtime data collection
 */
void initRuntimeDataCollection(void)
{
    int i;

    // Clear all data structures
    memset(&sBoardRuntimeData, 0, sizeof(SBoardData_t));
    memset(buBoardVersion, 0, sizeof(buBoardVersion));

    // Initialize min fields to 0xFFFF so first reading always wins
    sBoardRuntimeData.inputParams.inputMinVoltageRP = 0xFFFF;
    sBoardRuntimeData.inputParams.inputMinVoltageYP = 0xFFFF;
    sBoardRuntimeData.inputParams.inputMinVoltageBP = 0xFFFF;
    sBoardRuntimeData.inputParams.inputMinCurrentRP = 0xFFFF;
    sBoardRuntimeData.inputParams.inputMinCurrentYP = 0xFFFF;
    sBoardRuntimeData.inputParams.inputMinCurrentBP = 0xFFFF;
    sBoardRuntimeData.outputParams.outputMinVoltageR = 0xFFFF;
    sBoardRuntimeData.outputParams.outputMinVoltageY = 0xFFFF;
    sBoardRuntimeData.outputParams.outputMinVoltageB = 0xFFFF;
    sBoardRuntimeData.outputParams.outputMinCurrentR = 0xFFFF;
    sBoardRuntimeData.outputParams.outputMinCurrentY = 0xFFFF;
    sBoardRuntimeData.outputParams.outputMinCurrentB = 0xFFFF;

    // Clear frame tracking mask
    for (i = 0; i < FRAME_MASK_WORDS; i++)
    {
        frameReceivedMask[i] = 0;
    }

    // Clear runtime message flags
    memset(runtimeMessagePending, 0, sizeof(runtimeMessagePending));
    runtimeMessageReceived = false;

    allBUFramesReceived = false;
    totalFramesReceived = 0;
}

/**
 * @brief Reset runtime data collection for new cycle
 */
void resetRuntimeDataCollection(void)
{
    initRuntimeDataCollection();
}

/**
 * @brief Reset frame tracking only (keep data intact) for new 500ms cycle
 * Call at the start of each Slot 2 so frames are accepted again.
 */
void resetRuntimeFrameTracking(void)
{
    int i;

    for (i = 0; i < FRAME_MASK_WORDS; i++)
    {
        frameReceivedMask[i] = 0;
    }

    allBUFramesReceived = false;
    totalFramesReceived = 0;
}

/**
 * @brief Set bit in frame received mask
 * @param frameIndex Frame index (0-215)
 */
static void setBitInMask(uint16_t frameIndex)
{
    uint32_t wordIndex = frameIndex / 32;
    uint32_t bitIndex = frameIndex % 32;

    if (wordIndex < FRAME_MASK_WORDS)
    {
        frameReceivedMask[wordIndex] |= (1UL << bitIndex);
    }
}

/**
 * @brief Check if bit is set in frame received mask
 * @param frameIndex Frame index (0-215)
 * @return true if frame received, false otherwise
 */
static bool isBitSetInMask(uint16_t frameIndex)
{
    uint32_t wordIndex = frameIndex / 32;
    uint32_t bitIndex = frameIndex % 32;

    if (wordIndex < FRAME_MASK_WORDS)
    {
        return (frameReceivedMask[wordIndex] & (1UL << bitIndex)) != 0;
    }
    return false;
}

/**
 * @brief Process single BU board runtime frame
 * @param rxMsg Pointer to received CAN message
 *
 * Called directly from ISR for immediate buffer copy.
 * BU board IDs: FIRST_BU_ID to (FIRST_BU_ID + MAX_BU_BOARDS - 1)
 * Each board sends 18 frames (one per CT), total 216 frames.
 */
void processBURuntimeFrame(const MCAN_RxBufElement *rxMsg)
{
    BranchParameters_t *branchParam;
    uint8_t buBoardNo;
    uint8_t ctNo;
    uint16_t frameIndex;
    uint8_t *dataPtr;

    // Extract CAN ID first
    uint32_t boardId = (rxMsg->id >> 18U) & 0x1F;  // Extract CAN ID

    // Validate board ID range (IDs FIRST_BU_ID to FIRST_BU_ID + MAX_BU_BOARDS - 1)
    if (boardId < FIRST_BU_ID || boardId > (FIRST_BU_ID + MAX_BU_BOARDS - 1))
    {
        return;  // Invalid board ID
    }

    // Extract frame data
    dataPtr = (uint8_t *)rxMsg->data;

    // Store BU board firmware version (Byte 0) — one per board
    buBoardVersion[boardId - FIRST_BU_ID] = dataPtr[0];

    // Parse header (use only for processing, don't store)
    buBoardNo = dataPtr[2];  // BU board number (0-11)
    ctNo = dataPtr[3];       // CT number (1-18 from BU boards, 1-based)

    // Validate CT number (BU boards send 1-18)
    if (ctNo == 0 || ctNo > CTS_PER_BOARD)
    {
        return;  // Invalid CT number
    }

    // Convert 1-based CT number to 0-based index for array access
    // Calculate frame index (0-215) - board FIRST_BU_ID maps to index 0
    frameIndex = ((boardId - FIRST_BU_ID) * CTS_PER_BOARD) + (ctNo - 1);

    // Get pointer to branch parameter structure
    // Always overwrite — update whenever new data arrives
    branchParam = &sBoardRuntimeData.branchParams[frameIndex];

    // Copy branch data based on updated frame format (skip gaps in byte positions)
    branchParam->branchCurrent = (dataPtr[5] << 8) | dataPtr[4];
    branchParam->branchMaxCurrent = (dataPtr[7] << 8) | dataPtr[6];
    branchParam->branchMinCurrent = (dataPtr[9] << 8) | dataPtr[8];
    // Bytes 10-11: Load Percentage
    branchParam->branchLoadPercentage = (dataPtr[11] << 8) | dataPtr[10];
    branchParam->branchCurrentTHD = (dataPtr[13] << 8) | dataPtr[12];
    branchParam->branchVoltageTHD = (dataPtr[15] << 8) | dataPtr[14];
    branchParam->branchKW = (dataPtr[17] << 8) | dataPtr[16];
    branchParam->branchKVA = (dataPtr[19] << 8) | dataPtr[18];
    branchParam->branchKVAR = (dataPtr[21] << 8) | dataPtr[20];
    branchParam->branchAvgKW = (dataPtr[23] << 8) | dataPtr[22];
    branchParam->branchPowerFactor = (dataPtr[25] << 8) | dataPtr[24];
    branchParam->branchCurrentDemandHourly = (dataPtr[27] << 8) | dataPtr[26];
    branchParam->branchMaxCurrentDemand24Hour = (dataPtr[29] << 8) | dataPtr[28];
    branchParam->branchKWDemandHourly = (dataPtr[31] << 8) | dataPtr[30];
    branchParam->branchMaxKWDemand24Hour = (dataPtr[33] << 8) | dataPtr[32];
    branchParam->mccbStatus = dataPtr[34];
    branchParam->mccbTrip = dataPtr[35];

    // Track frame reception (only count first receive per cycle for completion detection)
    if (!isBitSetInMask(frameIndex))
    {
        setBitInMask(frameIndex);
        totalFramesReceived++;

        // Check if all 216 frames received this cycle
        if (totalFramesReceived >= TOTAL_BU_FRAMES)
        {
            allBUFramesReceived = true;
        }
    }
}

/**
 * @brief Process all pending runtime messages
 */
void processBURuntimeMessages(void)
{
    int i;

    for (i = 0; i < MCAN_RX_BUFF_NUM; i++)
    {
        if (runtimeMessagePending[i])
        {
            processBURuntimeFrame(&rxMsg[i]);
            runtimeMessagePending[i] = false;
        }
    }
}

/**
 * @brief Check if all BU frames have been received
 * @return true if all frames received, false otherwise
 */
bool checkAllBUFramesReceived(void)
{
    return allBUFramesReceived;
}

/**
 * @brief Apply calibration gain: raw * (gain / 10000)
 * Gains are stored as uint16_t with 10000 = 1.0x multiplier.
 * Uses minimal stack: single inline computation, no intermediate float locals.
 */
#define APPLY_GAIN(raw, gain) ((raw) * (gain) / 10000.0f)

/**
 * @brief Populate input parameters from S board ADC data
 * NOTE: Minimizes local float variables to avoid C2000 stack overflow.
 *       Uses only 3 reusable floats (tR, tY, tB) across computation blocks.
 */
void populateInputParameters(void)
{
    wait_for_minmax ++;
    if(wait_for_minmax>300)
    {
        MinMax_set=1;
        wait_for_minmax=301;
    }

    InputParameters_t *inputParam = &sBoardRuntimeData.inputParams;
    PDUData *cal = &pduManager.workingData.pduData;

    // Primary Indices: 3 (R), 4 (Y), 5 (B)
    phaseMetrology *pR = &gMetrologyWorkingData.phases[3];
    phaseMetrology *pY = &gMetrologyWorkingData.phases[4];
    phaseMetrology *pB = &gMetrologyWorkingData.phases[5];

    // 3 reusable float temporaries — keeps stack usage minimal on C2000
    float tR, tY, tB;

    // --- Voltage (Line-Neutral) with calibration gains ---
    tR = APPLY_GAIN(pR->readings.RMSVoltage, cal->primary.voltage.R);
    tY = APPLY_GAIN(pY->readings.RMSVoltage, cal->primary.voltage.Y);
    tB = APPLY_GAIN(pB->readings.RMSVoltage, cal->primary.voltage.B);

    inputParam->inputVoltageRP = (uint16_t)tR;
    inputParam->inputVoltageYP = (uint16_t)tY;
    inputParam->inputVoltageBP = (uint16_t)tB;
    inputParam->inputAverageVoltageLN = (uint16_t)((tR + tY + tB) / 3.0f);

    // Min/Max voltage tracking
    if ((uint16_t)tR > inputParam->inputMaxVoltageRP) inputParam->inputMaxVoltageRP = (uint16_t)tR;
    if ((uint16_t)tY > inputParam->inputMaxVoltageYP) inputParam->inputMaxVoltageYP = (uint16_t)tY;
    if ((uint16_t)tB > inputParam->inputMaxVoltageBP) inputParam->inputMaxVoltageBP = (uint16_t)tB;
    if ((uint16_t)tR > 0 && (uint16_t)tR < inputParam->inputMinVoltageRP) inputParam->inputMinVoltageRP = (uint16_t)tR;
    if ((uint16_t)tY > 0 && (uint16_t)tY < inputParam->inputMinVoltageYP) inputParam->inputMinVoltageYP = (uint16_t)tY;
    if ((uint16_t)tB > 0 && (uint16_t)tB < inputParam->inputMinVoltageBP) inputParam->inputMinVoltageBP = (uint16_t)tB;

    // --- Voltage (Line-Line) — reuse tR/tY/tB as calibrated L-N values ---
    {
        float vRY = gMetrologyWorkingData.LLReadings.LLRMSVoltage_ab;
        float vYB = gMetrologyWorkingData.LLReadings.LLRMSVoltage_bc;
        float vBR = gMetrologyWorkingData.LLReadings.LLRMSVoltage_ca;

        if (vRY == 0.0f) vRY = tR * 1.732051f;
        else vRY = APPLY_GAIN(vRY, cal->primary.voltage.R);
        if (vYB == 0.0f) vYB = tY * 1.732051f;
        else vYB = APPLY_GAIN(vYB, cal->primary.voltage.Y);
        if (vBR == 0.0f) vBR = tB * 1.732051f;
        else vBR = APPLY_GAIN(vBR, cal->primary.voltage.B);

        inputParam->inputVoltageRY = (uint16_t)vRY;
        inputParam->inputVoltageYB = (uint16_t)vYB;
        inputParam->inputVoltageBR = (uint16_t)vBR;
        inputParam->inputAverageVoltageLL = (uint16_t)((vRY + vYB + vBR) / 3.0f);
    }

    // --- Current with calibration gains ---
    tR = APPLY_GAIN(pR->readings.RMSCurrent, cal->primary.current.R);
    tY = APPLY_GAIN(pY->readings.RMSCurrent, cal->primary.current.Y);
    tB = APPLY_GAIN(pB->readings.RMSCurrent, cal->primary.current.B);

    inputParam->inputCurrentRP = (uint16_t)tR;
    inputParam->inputCurrentYP = (uint16_t)tY;
    inputParam->inputCurrentBP = (uint16_t)tB;
    inputParam->inputAverageCurrentLN = (uint16_t)((tR + tY + tB) / 3.0f);

    // Min/Max current tracking
    if(MinMax_set==1)
    {if ((uint16_t)tR > inputParam->inputMaxCurrentRP) inputParam->inputMaxCurrentRP = (uint16_t)tR;
    if ((uint16_t)tY > inputParam->inputMaxCurrentYP) inputParam->inputMaxCurrentYP = (uint16_t)tY;
    if ((uint16_t)tB > inputParam->inputMaxCurrentBP) inputParam->inputMaxCurrentBP = (uint16_t)tB;}

    if ((uint16_t)tR > 0 && (uint16_t)tR < inputParam->inputMinCurrentRP) inputParam->inputMinCurrentRP = (uint16_t)tR;
    if ((uint16_t)tY > 0 && (uint16_t)tY < inputParam->inputMinCurrentYP) inputParam->inputMinCurrentYP = (uint16_t)tY;
    if ((uint16_t)tB > 0 && (uint16_t)tB < inputParam->inputMinCurrentBP) inputParam->inputMinCurrentBP = (uint16_t)tB;

    // Neutral current with calibration gain
    {
        float neutralSq = tR*tR + tY*tY + tB*tB - tR*tY - tY*tB - tR*tB;
        float rawNeutral = (neutralSq > 0.0f) ? sqrtf(neutralSq) : 0.0f;
        inputParam->inputCurrentNeutral = (uint16_t)APPLY_GAIN(rawNeutral, cal->primary.neutralCurrent);
    }

    // Ground current with calibration gain (placeholder until ground CT available)
    inputParam->inputGroundCurrent = 0;

    // --- THD from FFT analyzer (already in percent, scale by 100 for uint16) ---
    inputParam->inputVoltageTHDRP = (uint16_t)(g_analyzer.results[THD_CH_IN_VOLT_R].thdPercent * 100.0f);
    inputParam->inputVoltageTHDYP = (uint16_t)(g_analyzer.results[THD_CH_IN_VOLT_Y].thdPercent * 100.0f);
    inputParam->inputVoltageTHDBP = (uint16_t)(g_analyzer.results[THD_CH_IN_VOLT_B].thdPercent * 100.0f);

    inputParam->inputCurrentTHDRP = (uint16_t)(g_analyzer.results[THD_CH_IN_CURR_R].thdPercent * 100.0f);
    inputParam->inputCurrentTHDYP = (uint16_t)(g_analyzer.results[THD_CH_IN_CURR_Y].thdPercent * 100.0f);
    inputParam->inputCurrentTHDBP = (uint16_t)(g_analyzer.results[THD_CH_IN_CURR_B].thdPercent * 100.0f);

    // --- Frequency from FFT (Scale by 100, no gain applied) ---
    inputParam->inputFrequency = (uint16_t)(g_analyzer.results[THD_CH_IN_VOLT_R].frequencyHz * 100.0f);

    // --- Power with calibration gain (reuse tR/tY/tB) ---
    tR = APPLY_GAIN(pR->readings.activePower, cal->primary.totalKW);
    tY = APPLY_GAIN(pY->readings.activePower, cal->primary.totalKW);
    tB = APPLY_GAIN(pB->readings.activePower, cal->primary.totalKW);

    inputParam->inputKWRP = (uint16_t)tR;
    inputParam->inputKWYP = (uint16_t)tY;
    inputParam->inputKWBP = (uint16_t)tB;
    inputParam->inputTotalKW = (uint16_t)(tR + tY + tB);

    inputParam->inputKVARP = (uint16_t)APPLY_GAIN(pR->readings.apparentPower, cal->primary.totalKW);
    inputParam->inputKVAYP = (uint16_t)APPLY_GAIN(pY->readings.apparentPower, cal->primary.totalKW);
    inputParam->inputKVABP = (uint16_t)APPLY_GAIN(pB->readings.apparentPower, cal->primary.totalKW);
    inputParam->inputTotalKVA = (uint16_t)APPLY_GAIN(pR->readings.apparentPower + pY->readings.apparentPower + pB->readings.apparentPower, cal->primary.totalKW);

    inputParam->inputKVARRP = (uint16_t)APPLY_GAIN(pR->readings.reactivePower, cal->primary.totalKW);
    inputParam->inputKVARYP = (uint16_t)APPLY_GAIN(pY->readings.reactivePower, cal->primary.totalKW);
    inputParam->inputKVARBP = (uint16_t)APPLY_GAIN(pB->readings.reactivePower, cal->primary.totalKW);
    inputParam->inputTotalKVAR = (uint16_t)APPLY_GAIN(pR->readings.reactivePower + pY->readings.reactivePower + pB->readings.reactivePower, cal->primary.totalKW);

    // --- Power Factor (Scale by 100, no gain applied) ---
    inputParam->inputPFRP = (uint16_t)(pR->readings.powerFactor * 100.0f);
    inputParam->inputPFYP = (uint16_t)(pY->readings.powerFactor * 100.0f);
    inputParam->inputPFBP = (uint16_t)(pB->readings.powerFactor * 100.0f);
    inputParam->inputPFAvg = (uint16_t)((pR->readings.powerFactor + pY->readings.powerFactor + pB->readings.powerFactor) / 3.0f * 100.0f);

    // --- Crest Factor ---
    // Computed by PhaseDetection_computeCrestFactors() from DMA buffers.
    // Do not overwrite here.
}

/**
 * @brief Populate output parameters from S board ADC data
 * NOTE: Minimizes local float variables to avoid C2000 stack overflow.
 *       Uses only 3 reusable floats (tR, tY, tB) across computation blocks.
 */
void populateOutputParameters(void)
{
    OutputParameters_t *outputParam = &sBoardRuntimeData.outputParams;
    PDUData *cal = &pduManager.workingData.pduData;

    // Secondary Indices: 0 (R), 1 (Y), 2 (B)
    phaseMetrology *pR = &gMetrologyWorkingData.phases[0];
    phaseMetrology *pY = &gMetrologyWorkingData.phases[1];
    phaseMetrology *pB = &gMetrologyWorkingData.phases[2];

    // 3 reusable float temporaries — keeps stack usage minimal on C2000
    float tR, tY, tB;

    // --- Voltage (Line-Neutral) with calibration gains ---
    tR = APPLY_GAIN(pR->readings.RMSVoltage, cal->secondary.voltage.R);
    tY = APPLY_GAIN(pY->readings.RMSVoltage, cal->secondary.voltage.Y);
    tB = APPLY_GAIN(pB->readings.RMSVoltage, cal->secondary.voltage.B);

    outputParam->outputVoltageRP = (uint16_t)tR;
    outputParam->outputVoltageYP = (uint16_t)tY;
    outputParam->outputVoltageBP = (uint16_t)tB;
    outputParam->outputAverageVoltageLN = (uint16_t)((tR + tY + tB) / 3.0f);

    // Min/Max voltage tracking
    if ((uint16_t)tR > outputParam->outputMaxVoltageR) outputParam->outputMaxVoltageR = (uint16_t)tR;
    if ((uint16_t)tY > outputParam->outputMaxVoltageY) outputParam->outputMaxVoltageY = (uint16_t)tY;
    if ((uint16_t)tB > outputParam->outputMaxVoltageB) outputParam->outputMaxVoltageB = (uint16_t)tB;
    if ((uint16_t)tR > 0 && (uint16_t)tR < outputParam->outputMinVoltageR) outputParam->outputMinVoltageR = (uint16_t)tR;
    if ((uint16_t)tY > 0 && (uint16_t)tY < outputParam->outputMinVoltageY) outputParam->outputMinVoltageY = (uint16_t)tY;
    if ((uint16_t)tB > 0 && (uint16_t)tB < outputParam->outputMinVoltageB) outputParam->outputMinVoltageB = (uint16_t)tB;

    // --- Voltage (Line-Line) — reuse tR/tY/tB as calibrated L-N values ---
    {
        float vRY = tR * 1.732051f;
        float vYB = tY * 1.732051f;
        float vBR = tB * 1.732051f;

        outputParam->outputVoltageRY = (uint16_t)vRY;
        outputParam->outputVoltageYB = (uint16_t)vYB;
        outputParam->outputVoltageBR = (uint16_t)vBR;
        outputParam->outputAverageVoltageLL = (uint16_t)((vRY + vYB + vBR) / 3.0f);
    }

    // --- Current with calibration gains ---
    tR = APPLY_GAIN(pR->readings.RMSCurrent, cal->secondary.current.R);
    tY = APPLY_GAIN(pY->readings.RMSCurrent, cal->secondary.current.Y);
    tB = APPLY_GAIN(pB->readings.RMSCurrent, cal->secondary.current.B);

    outputParam->outputCurrentRP = (uint16_t)tR;
    outputParam->outputCurrentYP = (uint16_t)tY;
    outputParam->outputCurrentBP = (uint16_t)tB;
    outputParam->outputAvgCurrent = (uint16_t)((tR + tY + tB) / 3.0f);

    // Min/Max current tracking
    if(MinMax_set==1)
    {if ((uint16_t)tR > outputParam->outputMaxCurrentR) outputParam->outputMaxCurrentR = (uint16_t)tR;
    if ((uint16_t)tY > outputParam->outputMaxCurrentY) outputParam->outputMaxCurrentY = (uint16_t)tY;
    if ((uint16_t)tB > outputParam->outputMaxCurrentB) outputParam->outputMaxCurrentB = (uint16_t)tB;}

    if ((uint16_t)tR > 0 && (uint16_t)tR < outputParam->outputMinCurrentR) outputParam->outputMinCurrentR = (uint16_t)tR;
    if ((uint16_t)tY > 0 && (uint16_t)tY < outputParam->outputMinCurrentY) outputParam->outputMinCurrentY = (uint16_t)tY;
    if ((uint16_t)tB > 0 && (uint16_t)tB < outputParam->outputMinCurrentB) outputParam->outputMinCurrentB = (uint16_t)tB;

    // Neutral current with calibration gain
    {
        float neutralSq = tR*tR + tY*tY + tB*tB - tR*tY - tY*tB - tR*tB;
        float rawNeutral = (neutralSq > 0.0f) ? sqrtf(neutralSq) : 0.0f;
        outputParam->outputCurrentNeutral = (uint16_t)APPLY_GAIN(rawNeutral, cal->secondary.neutralCurrent);
    }

    // --- Frequency from FFT (Scale by 100, no gain applied) ---
    outputParam->outputFrequencyRP = (uint16_t)(g_analyzer.results[THD_CH_OUT_VOLT_R].frequencyHz * 100.0f);
    outputParam->outputFrequencyYP = (uint16_t)(g_analyzer.results[THD_CH_OUT_VOLT_Y].frequencyHz * 100.0f);
    outputParam->outputFrequencyBP = (uint16_t)(g_analyzer.results[THD_CH_OUT_VOLT_B].frequencyHz * 100.0f);

    // --- THD from FFT analyzer (already in percent, scale by 100 for uint16) ---
    outputParam->outputVoltageTHDRP = (uint16_t)(g_analyzer.results[THD_CH_OUT_VOLT_R].thdPercent * 100.0f);
    outputParam->outputVoltageTHDYP = (uint16_t)(g_analyzer.results[THD_CH_OUT_VOLT_Y].thdPercent * 100.0f);
    outputParam->outputVoltageTHDBP = (uint16_t)(g_analyzer.results[THD_CH_OUT_VOLT_B].thdPercent * 100.0f);

    outputParam->outputCurrentTHDRP = (uint16_t)(g_analyzer.results[THD_CH_OUT_CURR_R].thdPercent * 100.0f);
    outputParam->outputCurrentTHDYP = (uint16_t)(g_analyzer.results[THD_CH_OUT_CURR_Y].thdPercent * 100.0f);
    outputParam->outputCurrentTHDBP = (uint16_t)(g_analyzer.results[THD_CH_OUT_CURR_B].thdPercent * 100.0f);
    outputParam->outputCurrentTHDNeutral = 0;

    // --- Crest Factor ---
    // Computed by PhaseDetection_computeCrestFactors() from DMA buffers.
    // Do not overwrite here.

    // --- Power with per-phase calibration gains (reuse tR/tY/tB) ---
    tR = APPLY_GAIN(pR->readings.activePower, cal->secondary.kw.R);
    tY = APPLY_GAIN(pY->readings.activePower, cal->secondary.kw.Y);
    tB = APPLY_GAIN(pB->readings.activePower, cal->secondary.kw.B);

    outputParam->outputKWPhaseR = (uint16_t)tR;
    outputParam->outputKWPhaseY = (uint16_t)tY;
    outputParam->outputKWPhaseB = (uint16_t)tB;
    outputParam->outputTotalKW = (uint16_t)(tR + tY + tB);

    outputParam->outputKVAPhaseR = (uint16_t)APPLY_GAIN(pR->readings.apparentPower, cal->secondary.kw.R);
    outputParam->outputKVAPhaseY = (uint16_t)APPLY_GAIN(pY->readings.apparentPower, cal->secondary.kw.Y);
    outputParam->outputKVAPhaseB = (uint16_t)APPLY_GAIN(pB->readings.apparentPower, cal->secondary.kw.B);
    outputParam->outputTotalKVA = (uint16_t)APPLY_GAIN(pR->readings.apparentPower + pY->readings.apparentPower + pB->readings.apparentPower, cal->secondary.kw.R);

    outputParam->outputKVARPhaseR = (uint16_t)APPLY_GAIN(pR->readings.reactivePower, cal->secondary.kw.R);
    outputParam->outputKVARPhaseY = (uint16_t)APPLY_GAIN(pY->readings.reactivePower, cal->secondary.kw.Y);
    outputParam->outputKVARPhaseB = (uint16_t)APPLY_GAIN(pB->readings.reactivePower, cal->secondary.kw.B);
    outputParam->outputTotalKVAR = (uint16_t)APPLY_GAIN(pR->readings.reactivePower + pY->readings.reactivePower + pB->readings.reactivePower, cal->secondary.kw.R);

    // --- Power Factor (Scale by 100, no gain applied) ---
    outputParam->outputPFRP = (uint16_t)(pR->readings.powerFactor * 100.0f);
    outputParam->outputPFYP = (uint16_t)(pY->readings.powerFactor * 100.0f);
    outputParam->outputPFBP = (uint16_t)(pB->readings.powerFactor * 100.0f);
    outputParam->outputPFAvg = (uint16_t)((pR->readings.powerFactor + pY->readings.powerFactor + pB->readings.powerFactor) / 3.0f * 100.0f);
}
