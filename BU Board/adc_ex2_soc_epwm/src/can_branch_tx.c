/*
 * can_branch_tx.c
 *
 * CAN Branch Data Transmission to S-Board
 * Implementation for sending IRMS, Power, PF for each CT/Branch
 *
 * Created on: Jan 31, 2026
 * Author: Auto-generated
 */

#include "can_branch_tx.h"
#include "can_bu.h"
#include "power_calc_3phase.h"
#include "calibration.h"
#include "metrology_structs.h"
#include <string.h>
#include <math.h>
#include "THD_Module/thd_analyzer.h"

extern THD_Analyzer g_analyzer;

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

/* Branch statistics for all 18 CTs */
BranchStats g_branchStats[BRANCH_TX_NUM_CTS];

/* S-Board and BU-Board IDs */
uint8_t g_sBoardId = 0;
uint8_t g_buBoardId = 0;

/* MCCB status for all branches */
uint8_t g_mccbStatus[BRANCH_TX_NUM_CTS];
uint8_t g_mccbTrip[BRANCH_TX_NUM_CTS];

/* DEBUG: All 18 branch TX messages - watch this in debugger */
BranchData_TxMsg g_branchTxData[BRANCH_TX_NUM_CTS];

/*
 * Branch-to-CT mapping table
 *
 * Physical wiring uses R,Y,B interleaved pattern:
 *   Branch 1  = CT1  (R1)    Branch 2  = CT7  (Y1)    Branch 3  = CT13 (B1)
 *   Branch 4  = CT2  (R2)    Branch 5  = CT8  (Y2)    Branch 6  = CT14 (B2)
 *   Branch 7  = CT3  (R3)    Branch 8  = CT9  (Y3)    Branch 9  = CT15 (B3)
 *   Branch 10 = CT4  (R4)    Branch 11 = CT10 (Y4)    Branch 12 = CT16 (B4)
 *   Branch 13 = CT5  (R5)    Branch 14 = CT11 (Y5)    Branch 15 = CT17 (B5)
 *   Branch 16 = CT6  (R6)    Branch 17 = CT12 (Y6)    Branch 18 = CT18 (B6)
 *
 * ADC reads phases grouped: CT1-6=R, CT7-12=Y, CT13-18=B
 * This table remaps to physical branch order for S-Board transmission.
 *
 * Index: branchNumber - 1 (0-17)
 * Value: ctNumber (1-18)
 */
static const uint16_t g_branchToCtMap[BRANCH_TX_NUM_CTS] = {
     1,  7, 13,   /* Branches 1-3:   R1, Y1, B1 */
     2,  8, 14,   /* Branches 4-6:   R2, Y2, B2 */
     3,  9, 15,   /* Branches 7-9:   R3, Y3, B3 */
     4, 10, 16,   /* Branches 10-12: R4, Y4, B4 */
     5, 11, 17,   /* Branches 13-15: R5, Y5, B5 */
     6, 12, 18    /* Branches 16-18: R6, Y6, B6 */
};

/* External reference to metrology data for IRMS */
extern metrologyData gMetrologyWorkingData;

/* External reference to power calculation results */
extern volatile Power3Phase_Results g_power3PhaseResults;

/* ========================================================================== */
/*                       Branch-to-CT Mapping                                 */
/* ========================================================================== */

uint16_t BranchTx_branchToCt(uint16_t branchNumber)
{
    if (branchNumber < 1 || branchNumber > BRANCH_TX_NUM_CTS)
    {
        return 0;
    }
    return g_branchToCtMap[branchNumber - 1];
}

/* ========================================================================== */
/*                          Internal Functions                                */
/* ========================================================================== */

/**
 * @brief   Pack a uint8_t into CAN data array at given byte position
 *          On C2000, each data[] element is 16-bit but MCAN uses only low 8 bits.
 */
static inline void pack8(uint16_t data[], uint16_t bytePos, uint8_t val)
{
    data[bytePos] = (uint16_t)(val & 0xFFU);
}

/**
 * @brief   Pack a uint16_t into CAN data array as two bytes (little-endian)
 *          Byte at bytePos = low byte, bytePos+1 = high byte
 */
static inline void pack16(uint16_t data[], uint16_t bytePos, uint16_t val)
{
    data[bytePos]     = (uint16_t)(val & 0xFFU);
    data[bytePos + 1] = (uint16_t)((val >> 8) & 0xFFU);
}

/**
 * @brief   Pack BranchData_TxMsg struct into MCAN TX data[] array
 *          Handles C2000 16-bit word to CAN byte conversion.
 *          MCAN_writeMsgRam reads only the low 8 bits of each data[] element.
 * @param   data    Pointer to txElement.data[] array
 * @param   txMsg   Pointer to the branch message struct
 */
static void BranchTx_packIntoCanData(uint16_t data[], const BranchData_TxMsg* txMsg)
{
    uint16_t i;

    /* Clear all 64 bytes (max CAN-FD payload) */
    for (i = 0; i < 64U; i++)
    {
        data[i] = 0U;
    }

    /* Header - single byte fields (Bytes 0-3) */
    pack8(data,  BRANCH_BYTE_VERSION,   txMsg->Version);
    pack8(data,  BRANCH_BYTE_S_BOARD,   txMsg->S_Board);
    pack8(data,  BRANCH_BYTE_BU_BOARD,  txMsg->BU_Board);
    pack8(data,  BRANCH_BYTE_CT_NO,     txMsg->CT_No);

    /* 16-bit fields packed as little-endian byte pairs */
    pack16(data, BRANCH_BYTE_CURRENT,           txMsg->Branch_Current);
    pack16(data, BRANCH_BYTE_MAX_CURRENT,       txMsg->Branch_MaxCurrent);
    pack16(data, BRANCH_BYTE_MIN_CURRENT,       txMsg->Branch_MinCurrent);
    pack16(data, BRANCH_BYTE_CURRENT_THD,       txMsg->Branch_Current_THD);
    pack16(data, BRANCH_BYTE_VOLTAGE_THD,       txMsg->Branch_Voltage_THD);
    pack16(data, BRANCH_BYTE_KW,                txMsg->Branch_KW);
    pack16(data, BRANCH_BYTE_KVA,               txMsg->Branch_KVA);
    pack16(data, BRANCH_BYTE_KVAR,              txMsg->Branch_KVAR);
    pack16(data, BRANCH_BYTE_AVG_KW,            txMsg->Branch_Avg_KW);
    pack16(data, BRANCH_BYTE_POWER_FACTOR,      txMsg->Branch_PowerFactor);
    pack16(data, BRANCH_BYTE_CURRENT_DEMAND,    txMsg->Branch_Current_Demand);
    pack16(data, BRANCH_BYTE_MAX_CURRENT_24H,   txMsg->Branch_MaxCurrent_Demand_24Hour);
    pack16(data, BRANCH_BYTE_KW_DEMAND_1H,      txMsg->Branch_KW_Demand_Hour);
    pack16(data, BRANCH_BYTE_MAX_KW_24H,        txMsg->Branch_Max_KW_Demand_24Hour);

    /* Single byte fields (Bytes 32-33) */
    pack8(data,  BRANCH_BYTE_MCCB_STATUS, txMsg->Branch_MCCB_Status);
    pack8(data,  BRANCH_BYTE_MCCB_TRIP,   txMsg->Branch_MCCB_Trip);

    /* Bytes 34-43: Reserved - already zeroed */
}

/**
 * @brief   Get CT power result from 3-phase power module
 * @param   ctNumber    CT number (1-18)
 * @return  Pointer to CT power result, or NULL if invalid
 */
static const CT_PowerResult* getCtPowerResult(uint16_t ctNumber)
{
    if (ctNumber < 1 || ctNumber > BRANCH_TX_NUM_CTS)
    {
        return NULL;
    }

    uint16_t ctIndex = ctNumber - 1;  /* Convert to 0-based index */
    uint16_t phase = ctIndex / POWER_CTS_PER_PHASE;
    uint16_t ctInPhase = ctIndex % POWER_CTS_PER_PHASE;

    switch (phase)
    {
        case POWER_PHASE_R:
            return &g_power3PhaseResults.rPhase.ct[ctInPhase];
        case POWER_PHASE_Y:
            return &g_power3PhaseResults.yPhase.ct[ctInPhase];
        case POWER_PHASE_B:
            return &g_power3PhaseResults.bPhase.ct[ctInPhase];
        default:
            return NULL;
    }
}

/**
 * @brief   Get IRMS for a CT from metrology data
 * @param   ctNumber    CT number (1-18)
 * @return  RMS current in Amps
 */
static float getCtIrms(uint16_t ctNumber)
{
    if (ctNumber < 1 || ctNumber > BRANCH_TX_NUM_CTS)
    {
        return 0.0f;
    }

    uint16_t ctIndex = ctNumber - 1;
    return gMetrologyWorkingData.phases[ctIndex].readings.RMSCurrent
           * g_calibration.currentGain[ctIndex];
}

/* ========================================================================== */
/*                          Public Functions                                  */
/* ========================================================================== */

void BranchTx_init(void)
{
    /* Reset all statistics */
    BranchTx_resetStats();

    /* Initialize MCCB status to default */
    uint16_t i;
    for (i = 0; i < BRANCH_TX_NUM_CTS; i++)
    {
        g_mccbStatus[i] = MCCB_STATUS_READY;
        g_mccbTrip[i] = MCCB_TRIP_NONE;
    }

    /* Get board IDs from CAN address (from DIP switches) */
    extern unsigned int address;
    g_buBoardId = (uint8_t)address;
    g_sBoardId = 0;  /* S-Board ID to be set from configuration */
}

void BranchTx_resetStats(void)
{
    memset(g_branchStats, 0, sizeof(g_branchStats));

    /* Initialize min values to max so first reading becomes minimum */
    uint16_t i;
    for (i = 0; i < BRANCH_TX_NUM_CTS; i++)
    {
        g_branchStats[i].currentMin = 9999.0f;
    }
}

void BranchTx_updateStats(uint16_t ctNumber, float current, float realPower)
{
    if (ctNumber < 1 || ctNumber > BRANCH_TX_NUM_CTS)
    {
        return;
    }

    uint16_t idx = ctNumber - 1;
    BranchStats* stats = &g_branchStats[idx];

    /* Update min current (only when current is meaningful, skip noise/zero) */
    if (current > Branch_Min_Mask && current < stats->currentMin)
    {
        stats->currentMin = current;
    }
    if (current > stats->currentMax)
    {
        stats->currentMax = current;
    }
    if (current > stats->currentMax24H)
    {
        stats->currentMax24H = current;
    }

    /* Update max kW (using absolute value for comparison) */
    float absKW = fabsf(realPower / 1000.0f);  /* Convert to kW */
    if (absKW > stats->kwMax24H)
    {
        stats->kwMax24H = absKW;
    }

    /* Update running average */
    stats->sampleCount++;
    float alpha = 1.0f / (float)stats->sampleCount;
    if (stats->sampleCount > 1000)
    {
        alpha = 0.001f;  /* Limit for stability */
    }
    stats->kwAverage = stats->kwAverage * (1.0f - alpha) + absKW * alpha;

    /* Update hourly demand (accumulate) */
    stats->currentDemandHourly += current;
    stats->kwDemand += absKW;
}

bool BranchTx_buildMessage(uint16_t branchNumber, BranchData_TxMsg* txMsg)
{
    if (branchNumber < 1 || branchNumber > BRANCH_TX_NUM_CTS || txMsg == NULL)
    {
        return false;
    }

    /* Map physical branch number to internal CT number */
    uint16_t ctNumber = g_branchToCtMap[branchNumber - 1];
    uint16_t ctIdx = ctNumber - 1;  /* 0-based index for CT data arrays */

    const CT_PowerResult* ctResult = getCtPowerResult(ctNumber);
    BranchStats* stats = &g_branchStats[ctIdx];

    /* Clear message */
    memset(txMsg, 0, sizeof(BranchData_TxMsg));

    /* Header (Bytes 0-3) - use branchNumber for S-Board */
    txMsg->Version = BRANCH_TX_VERSION_MAJOR;
    txMsg->S_Board = 1;//g_sBoardId;
    txMsg->BU_Board = g_buBoardId;
    txMsg->CT_No = (uint8_t)branchNumber;

    /* Current measurements (Bytes 4-11) - data from mapped CT */
    float irms = getCtIrms(ctNumber);
    txMsg->Branch_Current =(uint16_t)(irms * BRANCH_CURRENT_SCALE);
    txMsg->Branch_MaxCurrent = (uint16_t)(stats->currentMax * BRANCH_CURRENT_SCALE);
    /* If min hasn't been set yet (still at init value 9999), show current reading instead */
    if (stats->currentMin >= 9999.0f)
    {
        txMsg->Branch_MinCurrent = txMsg->Branch_Current;
    }
    else
    {
        txMsg->Branch_MinCurrent = (uint16_t)(stats->currentMin * BRANCH_CURRENT_SCALE);
    }
    /* THD measurements (Bytes 12-15) - from mapped CT */
    txMsg->Branch_Current_THD = (uint16_t)(g_analyzer.results[ctIdx].thdPercent * BRANCH_THD_SCALE);
    txMsg->Branch_Voltage_THD = 0;

    /* Power measurements (Bytes 16-25) - from mapped CT, kwGain applied on top of POWER_SCALE_3P */
    if (ctResult != NULL)
    {
        float kwGain = g_calibration.kwGain[ctIdx];

        /* Real Power in kW (0.1 kW units) */
        float kw = (ctResult->realPower_watts * kwGain) ;
        txMsg->Branch_KW = (uint16_t)(fabsf(kw) * BRANCH_POWER_SCALE);

        /* Apparent Power in kVA (0.1 kVA units) */
        float kva = (ctResult->apparentPower_VA * kwGain) ;
        txMsg->Branch_KVA = (uint16_t)(kva * BRANCH_POWER_SCALE);

        /* Reactive Power in kVAR (0.1 kVAR units) */
        float kvar = (ctResult->reactivePower_VAr * kwGain) ;
        txMsg->Branch_KVAR = (uint16_t)(fabsf(kvar) * BRANCH_POWER_SCALE);

        /* Power Factor (0.01 units, e.g., 98 = 0.98) - Bytes 24-25 */
        float pf = ctResult->powerFactor;
        if (pf < 0.0f) pf = -pf;  /* Use magnitude */
        txMsg->Branch_PowerFactor = (uint16_t)(pf * BRANCH_PF_SCALE);
    }

    /* Average kW (Bytes 22-23) */
    txMsg->Branch_Avg_KW = (uint16_t)(stats->kwAverage * BRANCH_POWER_SCALE);

    /* Demand history (Bytes 26-33) */
    /* Current demand is average, divide by sample count */
    if (stats->sampleCount > 0)
    {
        txMsg->Branch_Current_Demand = (uint16_t)((stats->currentDemandHourly / stats->sampleCount) * BRANCH_CURRENT_SCALE);
    }
    txMsg->Branch_MaxCurrent_Demand_24Hour = (uint16_t)(stats->currentMax24H * BRANCH_CURRENT_SCALE);
    txMsg->Branch_KW_Demand_Hour = (uint16_t)(stats->kwDemand * BRANCH_POWER_SCALE);
    txMsg->Branch_Max_KW_Demand_24Hour = (uint16_t)(stats->kwMax24H * BRANCH_POWER_SCALE);

    /* MCCB Status (Bytes 34-35) - from mapped CT */
    txMsg->Branch_MCCB_Status = g_mccbStatus[ctIdx];
    txMsg->Branch_MCCB_Trip = g_mccbTrip[ctIdx];

    /* Reserved field (Bytes 34-43) - already zeroed by memset */

    return true;
}

bool BranchTx_sendBranchData(uint16_t branchNumber)
{
    MCAN_TxBufElement txElement;
    uint16_t branchIdx = branchNumber - 1;

    /* Build message directly into global debug array (indexed by branch) */
    if (!BranchTx_buildMessage(branchNumber, &g_branchTxData[branchIdx]))
    {
        return false;
    }

    /* Configure CAN TX element */
    memset(&txElement, 0, sizeof(txElement));

    /* Standard ID: 10 + address so each BU board uses its own CAN ID */
    txElement.id = ((uint32_t)(10U + address) << 18U);
    txElement.xtd = 0U;  /* Standard ID */
    txElement.rtr = 0U;  /* Data frame */

    /* CAN-FD settings */
    txElement.fdf = 1U;  /* FD format */
    txElement.brs = 1U;  /* Bit rate switching */

    /* DLC 13 = 48 bytes in CAN-FD (minimum for 46-byte payload) */
    txElement.dlc = 15U;

    /* Pack struct fields into data[] byte-by-byte.
     * Cannot use memcpy on C2000: uint8_t is 16-bit, so memcpy copies
     * whole words and MCAN_writeMsgRam only uses the low 8 bits of each
     * data[] element, losing upper bytes of uint16_t fields. */
    BranchTx_packIntoCanData(txElement.data, &g_branchTxData[branchIdx]);

    /* Transmit */
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0U, &txElement);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();

    // Inter-frame delay: let bus idle between back-to-back branch frames
    // to prevent stuff errors when multiple BU boards TX simultaneously
    DEVICE_DELAY_US(100);
    return true;
}

void BranchTx_sendAllBranches(void)
{
    uint16_t branch;
    for (branch = 1; branch <= BRANCH_TX_NUM_CTS; branch++)
    {
        BranchTx_sendBranchData(branch);

        /* Small delay between messages to avoid bus congestion */
        DEVICE_DELAY_US(100);
    }
}

void BranchTx_hourlyReset(void)
{
    uint16_t i;
    for (i = 0; i < BRANCH_TX_NUM_CTS; i++)
    {
        /* Reset hourly statistics */
        g_branchStats[i].currentDemandHourly = 0.0f;
        g_branchStats[i].kwDemand = 0.0f;
        g_branchStats[i].sampleCount = 0;
    }
}

void BranchTx_dailyReset(void)
{
    uint16_t i;
    for (i = 0; i < BRANCH_TX_NUM_CTS; i++)
    {
        /* Reset 24-hour statistics */
        g_branchStats[i].currentMax24H = 0.0f;
        g_branchStats[i].kwMax24H = 0.0f;

        /* Also reset min/max */
        g_branchStats[i].currentMin = 9999.0f;
        g_branchStats[i].currentMax = 0.0f;
    }
}

void BranchTx_setMCCBStatus(uint16_t ctNumber, uint8_t status)
{
    if (ctNumber >= 1 && ctNumber <= BRANCH_TX_NUM_CTS)
    {
        g_mccbStatus[ctNumber - 1] = status;
    }
}

void BranchTx_setMCCBTrip(uint16_t ctNumber, uint8_t trip)
{
    if (ctNumber >= 1 && ctNumber <= BRANCH_TX_NUM_CTS)
    {
        g_mccbTrip[ctNumber - 1] = trip;
    }
}

uint16_t BranchTx_getCurrentTHD(uint16_t ctNumber)
{
    /* Placeholder - THD calculation not yet implemented */
    /* Returns 0 until THD module is added */
    (void)ctNumber;
    return 0;
}

uint16_t BranchTx_getVoltageTHD(uint16_t ctNumber)
{
    /* Placeholder - THD calculation not yet implemented */
    /* Returns 0 until THD module is added */
    (void)ctNumber;
    return 0;
}
