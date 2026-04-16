/*
 * phase_detection.h
 *
 * Consolidated Phase Detection Header
 * Contains all configurable thresholds for:
 *   1. Voltage Lack
 *   2. Voltage Unbalance
 *   3. Phase Loss
 *   4. Phase Sequence Error
 *   5. Crest Factor
 *
 * Data Source:  gMetrologyWorkingData  (metrology RMS readings)
 * Data Sink:   sBoardRuntimeData      (inputParams / outputParams status flags)
 *
 * Change thresholds here to tune detection sensitivity.
 */

#ifndef PHASE_DETECTION_H
#define PHASE_DETECTION_H

#include <stdint.h>
#include <float.h>
#include <stdlib.h>
#include "metrology.h"
#include "runtimedata.h"
#include "pdu_manager.h"
#include "THD_Module/thd_adc_driver.h"
#include "THD_Module/thd_config.h"
#include <math.h>

/* ============================================================
 *  CONFIGURABLE THRESHOLDS  (edit these values for tuning)
 * ============================================================ */

/*
 * 1. VOLTAGE LACK
 *    Detects when one L-L voltage differs from another L-L voltage
 *    by more than VOLT_LACK_THRESHOLD (in volts).
 *    Hysteresis: clears when difference drops below
 *    (VOLT_LACK_THRESHOLD - VOLT_LACK_HYSTERESIS).
 */
#define VOLT_LACK_THRESHOLD         5000      /* Volts */
#define VOLT_LACK_HYSTERESIS        1000      /* Volts */

/*
 * 2. VOLTAGE UNBALANCE
 *    Detects when any L-L voltage deviates from the average
 *    by more than VOLT_UNBALANCE_PERCENT (%).
 *    Formula: |V_phase - V_avg| / V_avg * 100 > threshold
 */
#define VOLT_UNBALANCE_PERCENT      2       /* Percent */

/*
 * 3. PHASE LOSS
 *    Detects when any L-L voltage drops below PHASE_LOSS_THRESHOLD.
 *    Clears when voltage recovers above
 *    (PHASE_LOSS_THRESHOLD + PHASE_LOSS_HYSTERESIS).
 */
#define PHASE_LOSS_THRESHOLD        35000      /* Volts */
#define PHASE_LOSS_HYSTERESIS       1000      /* Volts */

/*
 * 4. PHASE SEQUENCE ERROR
 *    Uses Clarke transform on instantaneous ADC samples.
 *    Cross-product of consecutive (alpha, beta) vectors
 *    determines rotation direction.
 *    Positive cross product = healthy (R->Y->B)
 *    Negative cross product = reversed (R->B->Y)
 *    PHASE_SEQ_VOTE_THRESHOLD: number of consecutive same-direction
 *    votes needed before changing status (debounce).
 */
#define PHASE_SEQ_VOTE_THRESHOLD    2000      /* Consecutive votes */

/* ============================================================
 *  FUNCTION PROTOTYPES
 * ============================================================ */

/*
 * Call this ONCE at startup to initialize internal state.
 */
void PhaseDetection_init(void);

/*
 * Individual detection functions.
 * Call these from the main loop after metrology RMS values are updated.
 * They read from gMetrologyWorkingData and write status flags
 * to sBoardRuntimeData.inputParams / outputParams.
 */
void PhaseDetection_voltageLack(void);
void PhaseDetection_voltageUnbalance(void);
void PhaseDetection_phaseLoss(void);

/*
 * Phase sequence error detection.
 * Call this from the ADC ISR with instantaneous voltage samples.
 * (offset-corrected, centered around 0)
 */
void PhaseDetection_phaseSequenceISR(int sample_R, int sample_Y, int sample_B);

/*
 * Crest Factor calculation from float32_t samples (THD buffers).
 * Auto-removes DC offset. Returns ~1.414 for a pure sine wave.
 */
float PhaseDetection_crestFactorF(const float32_t samples[], int num_samples);

/*
 * Compute crest factors for input and output voltages using THD buffers.
 * Input:  THD channels 6,7,8 (THD_CH_IN_VOLT_R/Y/B)
 * Output: THD channels 0,1,2 (THD_CH_OUT_VOLT_R/Y/B)
 * Call AFTER THD buffer is ready.
 */
void PhaseDetection_computeCrestFactors(void);

/*
 * Convenience function: calls voltageLack + voltageUnbalance + phaseLoss.
 * Call this from main loop after App_calculateMetrologyParameters().
 * (Phase sequence is handled separately in ISR.)
 */
void PhaseDetection_runAll(void);

/* ============================================================
 *  EXTERNAL REFERENCES
 * ============================================================ */
extern metrologyData       gMetrologyWorkingData;
extern SBoardData_t        sBoardRuntimeData;
extern PDUDataManager      pduManager;

#endif /* PHASE_DETECTION_H */
