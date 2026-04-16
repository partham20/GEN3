/*
 * power_calc_3phase.h
 *
 * Three-Phase Power Calculation Module for 18 CT Channels
 *
 * Calculates Real Power for all 18 CTs using:
 *   - 3 Voltage phases (R, Y, B) from CAN with ping-pong buffers
 *   - 18 Current channels from CT DMA with ping-pong buffers
 *
 * CT to Phase Mapping:
 *   R-Phase: CT1-CT6   use g_RPhaseVoltageBuffer
 *   Y-Phase: CT7-CT12  use g_YPhaseVoltageBuffer
 *   B-Phase: CT13-CT18 use g_BPhaseVoltageBuffer
 *
 * Real Power = (1/N) * SUM(V[i] * I[i]) for each CT
 */

#ifndef POWER_CALC_3PHASE_H_
#define POWER_CALC_3PHASE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "power_config.h"

/* ========================================================================== */
/*                                  Macros                                    */
/* ========================================================================== */

/* Number of CTs per phase */
#define POWER_CTS_PER_PHASE         (6U)

/* Total number of CTs */
#define POWER_TOTAL_CTS             (18U)

/* Number of phases */
#define POWER_NUM_PHASES            (3U)

/* Phase identifiers */
#define POWER_PHASE_R               (0U)
#define POWER_PHASE_Y               (1U)
#define POWER_PHASE_B               (2U)

/* Buffer size (from power_config.h) */
#define POWER_3P_BUFFER_SIZE        TOTAL_SAMPLE_COUNT

/* Scaling factors - adjust based on actual sensor calibration */
#define VOLTAGE_SCALE_3P            (1.0f)
#define CURRENT_SCALE_3P            (0.01388175f)
#define POWER_SCALE_3P              (VOLTAGE_SCALE_3P * CURRENT_SCALE_3P)

/* Low-pass filter coefficient for power smoothing */
#define POWER_FILTER_ALPHA          (0.1f)

/* ========================================================================== */
/*                             Type Definitions                               */
/* ========================================================================== */

/* Per-CT power calculation results */
typedef struct {
    int32_t  realPower_raw;         /* Real power in raw ADC units */
    float    realPower_watts;       /* Real power in Watts (P) */
    float    filteredPower;         /* Low-pass filtered power (internal) */
    int64_t  accumulator;           /* Raw accumulator (for debug) */
    float    apparentPower_VA;      /* Apparent power in VA (S = Vrms * Irms) */
    float    reactivePower_VAr;     /* Reactive power in VAr (Q = sqrt(S^2 - P^2)) */
    float    powerFactor;           /* Power factor (PF = P / S) */
} CT_PowerResult;

/* Per-phase power summary */
typedef struct {
    int32_t  totalPower_raw;        /* Sum of 6 CTs in raw units */
    float    totalPower_watts;      /* Sum of 6 CTs in Watts (P) */
    float    totalApparentPower_VA; /* Sum of 6 CTs apparent power in VA (S) */
    float    totalReactivePower_VAr;/* Sum of 6 CTs reactive power in VAr (Q) */
    float    phasePowerFactor;      /* Phase power factor (total P / total S) */
    CT_PowerResult ct[POWER_CTS_PER_PHASE];  /* Individual CT results */
} Phase_PowerResult;

/* Overall 3-phase power calculation status */
typedef struct {
    bool     isReady;               /* Calculation complete */
    bool     isValid;               /* Data valid */
    uint32_t sampleCount;           /* Samples per calculation */
    uint32_t cycleCount;            /* Number of calculation cycles */
} Power3Phase_Status;

/* Complete 3-phase power results */
typedef struct {
    Phase_PowerResult rPhase;       /* R-phase (CT1-CT6) */
    Phase_PowerResult yPhase;       /* Y-phase (CT7-CT12) */
    Phase_PowerResult bPhase;       /* B-phase (CT13-CT18) */
    int32_t  totalSystem_raw;       /* Total system power (all 18 CTs) */
    float    totalSystem_watts;     /* Total system power in Watts (P) */
    float    totalApparent_VA;      /* Total system apparent power in VA (S) */
    float    totalReactive_VAr;     /* Total system reactive power in VAr (Q) */
    float    systemPowerFactor;     /* System power factor (total P / total S) */
    float    vrmsR;                 /* R-phase VRMS from CAN (last R-frame bytes 62-63) */
    float    vrmsY;                 /* Y-phase VRMS from CAN (last Y-frame bytes 62-63) */
    float    vrmsB;                 /* B-phase VRMS from CAN (last B-frame bytes 62-63) */
} Power3Phase_Results;

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

/*
 * Ping-Pong Buffers for 3-Phase Voltage (from CAN)
 * These buffers receive voltage data from voltage_rx module
 */
extern volatile int16_t g_voltageRPhase_A[POWER_3P_BUFFER_SIZE];
extern volatile int16_t g_voltageRPhase_B[POWER_3P_BUFFER_SIZE];
extern volatile int16_t g_voltageYPhase_A[POWER_3P_BUFFER_SIZE];
extern volatile int16_t g_voltageYPhase_B[POWER_3P_BUFFER_SIZE];
extern volatile int16_t g_voltageBPhase_A[POWER_3P_BUFFER_SIZE];
extern volatile int16_t g_voltageBPhase_B[POWER_3P_BUFFER_SIZE];

/* Active buffer indicator for each voltage phase */
extern volatile uint16_t g_activeVoltageR;
extern volatile uint16_t g_activeVoltageY;
extern volatile uint16_t g_activeVoltageB;

/* Power calculation results */
extern volatile Power3Phase_Results g_power3PhaseResults;
extern volatile Power3Phase_Status g_power3PhaseStatus;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/**
 * @brief   Initialize the 3-phase power calculation module
 *          Clears all buffers and resets status
 */
void Power3Phase_init(void);

/**
 * @brief   Reset the 3-phase power calculation module
 */
void Power3Phase_reset(void);

/* ======================== Voltage Buffer Functions ======================== */

/**
 * @brief   Get pointer to active voltage buffer for a phase
 * @param   phase - POWER_PHASE_R, POWER_PHASE_Y, or POWER_PHASE_B
 * @return  Pointer to active (write) buffer
 */
volatile int16_t* Power3Phase_getActiveVoltageBuffer(uint16_t phase);

/**
 * @brief   Get pointer to inactive voltage buffer for a phase
 * @param   phase - POWER_PHASE_R, POWER_PHASE_Y, or POWER_PHASE_B
 * @return  Pointer to inactive (read) buffer
 */
volatile int16_t* Power3Phase_getInactiveVoltageBuffer(uint16_t phase);

/**
 * @brief   Swap voltage buffer for a specific phase
 * @param   phase - POWER_PHASE_R, POWER_PHASE_Y, or POWER_PHASE_B
 */
void Power3Phase_swapVoltageBuffer(uint16_t phase);

/**
 * @brief   Swap voltage buffers for all 3 phases
 */
void Power3Phase_swapAllVoltageBuffers(void);

/**
 * @brief   Copy voltage data from source buffer to active ping-pong buffer
 * @param   phase - POWER_PHASE_R, POWER_PHASE_Y, or POWER_PHASE_B
 * @param   srcBuffer - Source voltage data (already with DC offset removed)
 * @param   count - Number of samples to copy
 */
void Power3Phase_copyVoltageToActive(uint16_t phase, const volatile int16_t* srcBuffer, uint16_t count);

/* ======================== Power Calculation Functions ======================== */

/**
 * @brief   Calculate power for all 18 CTs using inactive voltage and current buffers
 *          Uses CT ping-pong buffers from ct_dma module
 *          Uses voltage ping-pong buffers from this module
 */
void Power3Phase_calculateAllPower(void);

/**
 * @brief   Calculate apparent power, reactive power and power factor for all 18 CTs
 *          Uses VRMS from CAN (stored in g_power3PhaseResults.vrms)
 *          Uses IRMS from gMetrologyWorkingData.phases[ctIndex].readings.RMSCurrent
 *          Must be called AFTER Power3Phase_calculateAllPower() and metrology calculations
 *
 * Formulas:
 *   Apparent Power (S) = Vrms * Irms  [VA]
 *   Reactive Power (Q) = sqrt(S^2 - P^2)  [VAr]
 *   Power Factor (PF) = P / S  [unitless, -1 to +1]
 */
void Power3Phase_calculateApparentReactivePF(void);

/**
 * @brief   Extract and store VRMS from CAN frame (bytes 62-63) for a specific phase
 *          Call this for the last frame of each phase (R, Y, B)
 * @param   frameData - Pointer to the CAN frame data (uint16_t array)
 *                      Will be cast to uint16_t* for 8-bit byte access
 * @param   phaseId - Phase identifier (PHASE_ID_R=1, PHASE_ID_Y=2, PHASE_ID_B=3)
 *
 * Note: VRMS is divided by 100 to get values like 230.xx volts
 *       (e.g., CAN sends 23050 -> VRMS = 230.50V)
 */
void Power3Phase_extractVrmsFromCAN(const uint16_t* frameData, uint16_t phaseId);

/**
 * @brief   Calculate power for a single CT
 * @param   ctNumber - CT number (1-18)
 * @param   voltageBuffer - Pointer to voltage samples
 * @param   currentBuffer - Pointer to current samples
 * @return  Real power in raw ADC units
 */
int32_t Power3Phase_calculateCTPower(uint16_t ctNumber,
                                      const volatile int16_t* voltageBuffer,
                                      const volatile int16_t* currentBuffer);

/* ======================== Result Access Functions ======================== */

/**
 * @brief   Get power result for a specific CT
 * @param   ctNumber - CT number (1-18)
 * @return  Pointer to CT power result structure
 */
const CT_PowerResult* Power3Phase_getCTResult(uint16_t ctNumber);

/**
 * @brief   Get power result for a specific phase
 * @param   phase - POWER_PHASE_R, POWER_PHASE_Y, or POWER_PHASE_B
 * @return  Pointer to phase power result structure
 */
const Phase_PowerResult* Power3Phase_getPhaseResult(uint16_t phase);

/**
 * @brief   Get total system power (all 18 CTs) in raw units
 * @return  Total power in raw ADC units
 */
int32_t Power3Phase_getTotalPower_raw(void);

/**
 * @brief   Get total system power (all 18 CTs) in Watts
 * @return  Total power in Watts
 */
float Power3Phase_getTotalPower_watts(void);

/**
 * @brief   Get power for a specific CT in raw units
 * @param   ctNumber - CT number (1-18)
 * @return  CT power in raw ADC units
 */
int32_t Power3Phase_getCTPower_raw(uint16_t ctNumber);

/**
 * @brief   Get power for a specific CT in Watts
 * @param   ctNumber - CT number (1-18)
 * @return  CT power in Watts
 */
float Power3Phase_getCTPower_watts(uint16_t ctNumber);

/**
 * @brief   Get total phase power in raw units
 * @param   phase - POWER_PHASE_R, POWER_PHASE_Y, or POWER_PHASE_B
 * @return  Phase total power in raw ADC units
 */
int32_t Power3Phase_getPhaseTotalPower_raw(uint16_t phase);

/**
 * @brief   Get total phase power in Watts
 * @param   phase - POWER_PHASE_R, POWER_PHASE_Y, or POWER_PHASE_B
 * @return  Phase total power in Watts
 */
float Power3Phase_getPhaseTotalPower_watts(uint16_t phase);

/**
 * @brief   Check if power calculation is ready
 * @return  true if calculation complete
 */
bool Power3Phase_isReady(void);

/**
 * @brief   Get calculation cycle count
 * @return  Number of completed calculation cycles
 */
uint32_t Power3Phase_getCycleCount(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_CALC_3PHASE_H_ */
