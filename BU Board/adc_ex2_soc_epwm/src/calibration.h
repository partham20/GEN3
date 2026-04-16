/*
 * calibration.h
 *
 * Per-CT Calibration Gains for 18 Branches
 *
 * These gains are multiplied ON TOP of the existing scaling defines
 * (POWER_SCALE_3P, CURRENT_SCALE_3P, etc.) for fine-tuning each branch.
 *
 * Current Gain: multiplied with IRMS at runtime
 *   calibrated_irms = raw_irms * currentGain[ct]
 *
 * KW Gain: multiplied with real power (Watts) at runtime
 *   calibrated_watts = raw_watts * kwGain[ct]
 *
 * Default value = 1.0f (no adjustment)
 *
 * Created on: Feb 16, 2026
 */

#ifndef SRC_CALIBRATION_H_
#define SRC_CALIBRATION_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Number of CT channels */
#define CAL_NUM_CTS     (18U)

/* Gain encoding scale: uint16_t 1000 = 1.0x gain, 1500 = 1.5x gain, etc. */
#define CAL_GAIN_SCALE      (10000U)
#define CAL_TX_GAIN_SCALE   CAL_GAIN_SCALE
#define CAL_RX_GAIN_SCALE   CAL_GAIN_SCALE

/* Per-CT calibration gains */
typedef struct {
    float currentGain[CAL_NUM_CTS];   /* Multiplied with IRMS (on top of CURRENT_SCALE) */
    float kwGain[CAL_NUM_CTS];        /* Multiplied with real power watts (on top of POWER_SCALE_3P) */
} CalibrationGains;

/* Global calibration instance */
extern CalibrationGains g_calibration;

/**
 * @brief   Initialize all calibration gains
 *          Sets all 18 current and KW gains to their default values
 *          Call this once during startup before power calculations
 */
void Calibration_init(void);

/**
 * @brief   Set current gain for a specific CT
 * @param   ctNumber    CT number (1-18)
 * @param   gain        Gain multiplier (1.0 = no change)
 */
void Calibration_setCurrentGain(uint16_t ctNumber, float gain);

/**
 * @brief   Set KW gain for a specific CT
 * @param   ctNumber    CT number (1-18)
 * @param   gain        Gain multiplier (1.0 = no change)
 */
void Calibration_setKWGain(uint16_t ctNumber, float gain);

/**
 * @brief   Get current gain for a specific CT
 * @param   ctNumber    CT number (1-18)
 * @return  Current gain multiplier
 */
float Calibration_getCurrentGain(uint16_t ctNumber);

/**
 * @brief   Get KW gain for a specific CT
 * @param   ctNumber    CT number (1-18)
 * @return  KW gain multiplier
 */
float Calibration_getKWGain(uint16_t ctNumber);

#ifdef __cplusplus
}
#endif

#endif /* SRC_CALIBRATION_H_ */
