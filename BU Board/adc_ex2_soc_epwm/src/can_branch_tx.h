/*
 * can_branch_tx.h
 *
 * CAN Branch Data Transmission to S-Board
 * Sends IRMS, Power (Real, Reactive, Apparent), PF for each CT/Branch
 *
 * Created on: Jan 31, 2026
 * Author: Auto-generated
 */

#ifndef SRC_CAN_BRANCH_TX_H_
#define SRC_CAN_BRANCH_TX_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "power_config.h"
#include "power_calc_3phase.h"

/* ========================================================================== */
/*                              Macros                                        */
/* ========================================================================== */

/* CAN Message ID for Branch Data TX */
#define CAN_ID_BRANCH_TX            (11U)

/* Message size in CAN bus bytes (NOT C2000 words - do not use with memcpy) */
#define CAN_BRANCH_MSG_SIZE         (44U)

/* Version info */
#define BRANCH_TX_VERSION_MAJOR     (6U)
#define BRANCH_TX_VERSION_MINOR     (0U)

/* Number of branches (CTs) */
#define BRANCH_TX_NUM_CTS           (18U)

/* Scaling factors for fixed-point transmission */
#define BRANCH_CURRENT_SCALE        (100U)    /* Current in 0.01A units */
#define BRANCH_POWER_SCALE          (0.1f)     /* Power in 0.1 kW/kVA/kVAR units */
#define BRANCH_PF_SCALE             (100U)    /* PF in 0.01 units (e.g., 98 = 0.98) */
#define BRANCH_THD_SCALE            (100U)     /* THD in 0.1% units */

/* ========================================================================== */
/*                         Byte Position Definitions                          */
/* ========================================================================== */

/* Byte positions in CAN message */
#define BRANCH_BYTE_VERSION             (0U)     /* Byte 0: Version */
#define BRANCH_BYTE_S_BOARD             (1U)     /* Byte 1: S-Board ID */
#define BRANCH_BYTE_BU_BOARD            (2U)     /* Byte 2: BU-Board ID */
#define BRANCH_BYTE_CT_NO               (3U)     /* Byte 3: CT Number */
#define BRANCH_BYTE_CURRENT             (4U)     /* Bytes 4-5: Branch Current (IRMS) */
#define BRANCH_BYTE_MAX_CURRENT         (6U)     /* Bytes 6-7: Max Current */
#define BRANCH_BYTE_MIN_CURRENT         (8U)     /* Bytes 8-9: Min Current */
#define BRANCH_BYTE_CURRENT_THD         (10U)    /* Bytes 10-11: Current THD */
#define BRANCH_BYTE_VOLTAGE_THD         (12U)    /* Bytes 12-13: Voltage THD */
#define BRANCH_BYTE_KW                  (14U)    /* Bytes 14-15: Real Power (kW) */
#define BRANCH_BYTE_KVA                 (16U)    /* Bytes 16-17: Apparent Power (kVA) */
#define BRANCH_BYTE_KVAR                (18U)    /* Bytes 18-19: Reactive Power (kVAR) */
#define BRANCH_BYTE_AVG_KW              (20U)    /* Bytes 20-21: Avg kW */
#define BRANCH_BYTE_POWER_FACTOR        (22U)    /* Bytes 22-23: Power Factor */
#define BRANCH_BYTE_CURRENT_DEMAND      (24U)    /* Bytes 24-25: Current Demand */
#define BRANCH_BYTE_MAX_CURRENT_24H     (26U)    /* Bytes 26-27: Max Current Demand 24hr */
#define BRANCH_BYTE_KW_DEMAND_1H        (28U)    /* Bytes 28-29: kW Demand Hourly */
#define BRANCH_BYTE_MAX_KW_24H          (30U)    /* Bytes 30-31: Max kW Demand 24hr */
#define BRANCH_BYTE_MCCB_STATUS         (32U)    /* Byte 32: MCCB Status */
#define BRANCH_BYTE_MCCB_TRIP           (33U)    /* Byte 33: MCCB Trip */
#define BRANCH_BYTE_RESERVED            (34U)    /* Bytes 34-43: Reserved (10 bytes) */

#define Branch_Min_Mask                  1.0f
/* ========================================================================== */
/*                           Type Definitions                                 */
/* ========================================================================== */

/**
 * @brief Branch Data Structure for CAN Transmission (44 bytes)
 *        Packed structure matching the byte layout for S-Board communication
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  Version;
    uint8_t  S_Board;
    uint8_t  BU_Board;
    uint8_t  CT_No;
    uint16_t Branch_Current;
    uint16_t Branch_MaxCurrent;
    uint16_t Branch_MinCurrent;
    uint16_t Branch_Current_THD;
    uint16_t Branch_Voltage_THD;
    uint16_t Branch_KW;
    uint16_t Branch_KVA;
    uint16_t Branch_KVAR;
    uint16_t Branch_Avg_KW;
    uint16_t Branch_PowerFactor;
    uint16_t Branch_Current_Demand;
    uint16_t Branch_MaxCurrent_Demand_24Hour;
    uint16_t Branch_KW_Demand_Hour;
    uint16_t Branch_Max_KW_Demand_24Hour;
    uint8_t  Branch_MCCB_Status;
    uint8_t  Branch_MCCB_Trip;
    uint8_t  Reserved[10];
} BranchData_TxMsg;
#pragma pack(pop)

/**
 * @brief Branch Statistics (tracking min/max/demand over time)
 */
typedef struct {
    float    currentMin;                /* Minimum current seen */
    float    currentMax;                /* Maximum current seen */
    float    currentDemandHourly;       /* Current demand (hourly average) */
    float    currentMax24H;             /* Maximum current in 24 hours */
    float    kwMax24H;                  /* Maximum kW in 24 hours */
    float    kwAverage;                 /* Running average kW */
    float    kwDemand;                  /* kW demand value */
    uint32_t sampleCount;               /* Sample count for averaging */
    uint32_t hourlyResetTimestamp;      /* Timestamp for hourly reset */
    uint32_t dailyResetTimestamp;       /* Timestamp for 24hr reset */
} BranchStats;

/**
 * @brief MCCB Status Bit Definitions
 */
typedef enum {
    MCCB_STATUS_CLOSED      = 0x0001,   /* MCCB is closed */
    MCCB_STATUS_OPEN        = 0x0002,   /* MCCB is open */
    MCCB_STATUS_FAULT       = 0x0004,   /* MCCB in fault condition */
    MCCB_STATUS_REMOTE      = 0x0008,   /* Remote control enabled */
    MCCB_STATUS_LOCAL       = 0x0010,   /* Local control enabled */
    MCCB_STATUS_READY       = 0x0020,   /* MCCB ready to operate */
} MCCB_StatusFlags;

/**
 * @brief MCCB Trip Bit Definitions
 */
typedef enum {
    MCCB_TRIP_NONE          = 0x0000,   /* No trip */
    MCCB_TRIP_OVERCURRENT   = 0x0001,   /* Overcurrent trip */
    MCCB_TRIP_SHORT_CIRCUIT = 0x0002,   /* Short circuit trip */
    MCCB_TRIP_GROUND_FAULT  = 0x0004,   /* Ground fault trip */
    MCCB_TRIP_UNDERVOLTAGE  = 0x0008,   /* Undervoltage trip */
    MCCB_TRIP_OVERVOLTAGE   = 0x0010,   /* Overvoltage trip */
    MCCB_TRIP_THERMAL       = 0x0020,   /* Thermal overload trip */
    MCCB_TRIP_MANUAL        = 0x0040,   /* Manual trip */
} MCCB_TripFlags;

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

/* Branch statistics for all 18 CTs */
extern BranchStats g_branchStats[BRANCH_TX_NUM_CTS];

/* S-Board and BU-Board IDs (set from DIP switches or configuration) */
extern uint8_t g_sBoardId;
extern uint8_t g_buBoardId;

/* MCCB status for all branches */
extern uint8_t g_mccbStatus[BRANCH_TX_NUM_CTS];
extern uint8_t g_mccbTrip[BRANCH_TX_NUM_CTS];

/* DEBUG: All 18 branch TX messages - watch this in CCS debugger */
extern BranchData_TxMsg g_branchTxData[BRANCH_TX_NUM_CTS];

/* ========================================================================== */
/*                        Function Declarations                               */
/* ========================================================================== */

/**
 * @brief   Initialize the branch TX module
 *          Sets up CAN TX and initializes statistics
 */
void BranchTx_init(void);

/**
 * @brief   Reset all branch statistics
 */
void BranchTx_resetStats(void);

/**
 * @brief   Update branch statistics with new readings
 *          Call this periodically after power calculations
 * @param   ctNumber    CT number (1-18)
 * @param   current     Current IRMS value in Amps
 * @param   realPower   Real power in Watts
 */
void BranchTx_updateStats(uint16_t ctNumber, float current, float realPower);

/**
 * @brief   Build CAN message for a specific branch/CT
 * @param   ctNumber    CT number (1-18)
 * @param   txMsg       Pointer to message structure to fill
 * @return  true if successful, false if invalid CT number
 */
bool BranchTx_buildMessage(uint16_t ctNumber, BranchData_TxMsg* txMsg);

/**
 * @brief   Send branch data for a specific CT via CAN
 * @param   ctNumber    CT number (1-18)
 * @return  true if transmission started successfully
 */
bool BranchTx_sendBranchData(uint16_t ctNumber);

/**
 * @brief   Send branch data for all 18 CTs via CAN
 *          Sends one message per CT sequentially
 */
void BranchTx_sendAllBranches(void);

/**
 * @brief   Process hourly reset for demand calculations
 *          Call this every hour to reset hourly statistics
 */
void BranchTx_hourlyReset(void);

/**
 * @brief   Process daily reset for 24-hour demand calculations
 *          Call this every 24 hours to reset daily statistics
 */
void BranchTx_dailyReset(void);

/**
 * @brief   Set MCCB status for a branch
 * @param   ctNumber    CT number (1-18)
 * @param   status      MCCB status flags (see MCCB_StatusFlags)
 */
void BranchTx_setMCCBStatus(uint16_t ctNumber, uint8_t status);

/**
 * @brief   Set MCCB trip for a branch
 * @param   ctNumber    CT number (1-18)
 * @param   trip        MCCB trip flags (see MCCB_TripFlags)
 */
void BranchTx_setMCCBTrip(uint16_t ctNumber, uint8_t trip);

/**
 * @brief   Get current THD for a branch (placeholder - needs THD calculation)
 * @param   ctNumber    CT number (1-18)
 * @return  Current THD in 0.1% units
 */
uint16_t BranchTx_getCurrentTHD(uint16_t ctNumber);

/**
 * @brief   Get voltage THD for a branch (placeholder - needs THD calculation)
 * @param   ctNumber    CT number (1-18)
 * @return  Voltage THD in 0.1% units
 */
uint16_t BranchTx_getVoltageTHD(uint16_t ctNumber);

#ifdef __cplusplus
}
#endif

#endif /* SRC_CAN_BRANCH_TX_H_ */
