/*
 * calibration.c
 *
 * Per-CT Calibration Gains for 18 Branches
 *
 * These gains are applied ON TOP of existing scaling defines:
 *   - POWER_SCALE_3P (0.01388175) remains unchanged
 *   - CURRENT_SCALE_3P remains unchanged
 *   - Calibration gains provide per-CT fine-tuning
 *
 * To calibrate a specific CT:
 *   - If CT3 reads 2% low on current:  set currentGain[2] = 1.02f
 *   - If CT5 reads 3% high on power:   set kwGain[4] = 0.97f
 *
 * Created on: Feb 16, 2026
 */

#include "calibration.h"

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

CalibrationGains g_calibration;

/* ========================================================================== */
/*                       Default Calibration Values                           */
/* ========================================================================== */

/*
 * Default gains for all 18 CTs
 * Adjust these values per-CT after field calibration
 * Index 0 = CT1, Index 1 = CT2, ... Index 17 = CT18
 *
 * R-Phase: CT1(idx0) - CT6(idx5)
 * Y-Phase: CT7(idx6) - CT12(idx11)
 * B-Phase: CT13(idx12) - CT18(idx17)
 */
static const float defaultCurrentGain[CAL_NUM_CTS] = {
    /* R-Phase: CT1 - CT6 */
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    /* Y-Phase: CT7 - CT12 */
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    /* B-Phase: CT13 - CT18 */
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};

static const float defaultKWGain[CAL_NUM_CTS] = {
    /* R-Phase: CT1 - CT6 */
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    /* Y-Phase: CT7 - CT12 */
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    /* B-Phase: CT13 - CT18 */
    1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f
};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

void Calibration_init(void)
{
    uint16_t i;

    for(i = 0U; i < CAL_NUM_CTS; i++)
    {
        g_calibration.currentGain[i] = defaultCurrentGain[i];
        g_calibration.kwGain[i] = defaultKWGain[i];
    }
}

void Calibration_setCurrentGain(uint16_t ctNumber, float gain)
{
    if(ctNumber >= 1U && ctNumber <= CAL_NUM_CTS)
    {
        g_calibration.currentGain[ctNumber - 1U] = gain;
    }
}

void Calibration_setKWGain(uint16_t ctNumber, float gain)
{
    if(ctNumber >= 1U && ctNumber <= CAL_NUM_CTS)
    {
        g_calibration.kwGain[ctNumber - 1U] = gain;
    }
}

float Calibration_getCurrentGain(uint16_t ctNumber)
{
    if(ctNumber >= 1U && ctNumber <= CAL_NUM_CTS)
    {
        return g_calibration.currentGain[ctNumber - 1U];
    }
    return 1.0f;
}

float Calibration_getKWGain(uint16_t ctNumber)
{
    if(ctNumber >= 1U && ctNumber <= CAL_NUM_CTS)
    {
        return g_calibration.kwGain[ctNumber - 1U];
    }
    return 1.0f;
}
