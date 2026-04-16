/*
 * phase_detection.c
 *
 * Consolidated Phase Detection Module
 * All 4 protection algorithms in one file:
 *   1. Voltage Lack        - L-L voltage difference between phases
 *   2. Voltage Unbalance   - Deviation from average L-L voltage
 *   3. Phase Loss          - L-L voltage below threshold
 *   4. Phase Sequence Error - Clarke transform rotation detection
 *
 * DATA SOURCE: gMetrologyWorkingData (metrology library RMS values)
 *   Primary  (Input):  phases[0]=R, phases[1]=Y, phases[2]=B
 *                       LLReadings: LLRMSVoltage_ab(RY), _bc(YB), _ca(BR)
 *   Secondary (Output): phases[3]=R, phases[4]=Y, phases[5]=B
 *                       L-L computed from L-N: sqrt(3) * V_LN
 *
 * DATA SINK: sBoardRuntimeData.inputParams / .outputParams
 *   status fields: voltageLack, voltageUnbalance, phaseLoss, phaseSequenceError
 *
 * CALIBRATION: Uses pduManager.workingData.pduData gains via APPLY_GAIN macro.
 *
 * STATUS FLAGS: 0 = Healthy, 1 = Fault
 */

#include "phase_detection.h"

/* Calibration gain macro (same as runtimedata.c) */
#define APPLY_GAIN(raw, gain) ((raw) * (gain) / 10000.0f)

/* ============================================================
 *  INTERNAL STATE FOR PHASE SEQUENCE DETECTION
 * ============================================================ */
static float prev_v_alpha = 0.0f;
static float prev_v_beta  = 0.0f;
static int   seq_vote_counter = 0;
static uint16_t phase_seq_status = 0;

float vRY, vYB, vBR;
float v_alpha, v_beta;
float cross;

/* Helper: absolute value for float */
static float fabsval(float x)
{
    return (x < 0.0f) ? -x : x;
}

/*
 * Helper: Get calibrated primary L-L voltages from metrology.
 * L-L = sqrt(3) * L-N for each phase.
 */
static void getPrimaryLL(float *vRY, float *vYB, float *vBR)
{
//    PDUData *cal = &pduManager.workingData.pduData;
//
//    /* L-N voltages with calibration */
//    float tR = APPLY_GAIN(gMetrologyWorkingData.phases[3].readings.RMSVoltage, cal->primary.voltage.R);
//    float tY = APPLY_GAIN(gMetrologyWorkingData.phases[4].readings.RMSVoltage, cal->primary.voltage.Y);
//    float tB = APPLY_GAIN(gMetrologyWorkingData.phases[5].readings.RMSVoltage, cal->primary.voltage.B);
//
//    /* L-L = sqrt(3) * L-N */
//    *vRY = 1.7320508f * tR;
//    *vYB = 1.7320508f * tY;
//    *vBR = 1.7320508f * tB;
    PDUData *cal = &pduManager.workingData.pduData;

    /* L-N voltages with calibration */
    float tR = APPLY_GAIN(gMetrologyWorkingData.phases[3].readings.RMSVoltage, cal->primary.voltage.R);
    float tY = APPLY_GAIN(gMetrologyWorkingData.phases[4].readings.RMSVoltage, cal->primary.voltage.Y);
    float tB = APPLY_GAIN(gMetrologyWorkingData.phases[5].readings.RMSVoltage, cal->primary.voltage.B);

    /* L-L from metrology LLReadings, fallback to L-N formula */
    float rawRY = gMetrologyWorkingData.LLReadings.LLRMSVoltage_ab;
    float rawYB = gMetrologyWorkingData.LLReadings.LLRMSVoltage_bc;
    float rawBR = gMetrologyWorkingData.LLReadings.LLRMSVoltage_ca;

    *vRY = (rawRY == 0.0f) ? sqrtf(tR * tR + tY * tY + tR * tY)
                            : APPLY_GAIN(rawRY, cal->primary.voltage.R);
    *vYB = (rawYB == 0.0f) ? sqrtf(tY * tY + tB * tB + tY * tB)
                            : APPLY_GAIN(rawYB, cal->primary.voltage.Y);
    *vBR = (rawBR == 0.0f) ? sqrtf(tB * tB + tR * tR + tB * tR)
                            : APPLY_GAIN(rawBR, cal->primary.voltage.B);

}

/*
 * Helper: Get calibrated secondary L-L voltages from metrology.
 * L-L = sqrt(3) * L-N for each phase.
 */
static void getSecondaryLL(float *vRY, float *vYB, float *vBR)
{
//    PDUData *cal = &pduManager.workingData.pduData;
//
//    float tR = APPLY_GAIN(gMetrologyWorkingData.phases[0].readings.RMSVoltage, cal->secondary.voltage.R);
//    float tY = APPLY_GAIN(gMetrologyWorkingData.phases[1].readings.RMSVoltage, cal->secondary.voltage.Y);
//    float tB = APPLY_GAIN(gMetrologyWorkingData.phases[2].readings.RMSVoltage, cal->secondary.voltage.B);
//
//    /* L-L = sqrt(3) * L-N */
//    *vRY = 1.7320508f * tR;
//    *vYB = 1.7320508f * tY;
//    *vBR = 1.7320508f * tB;
    PDUData *cal = &pduManager.workingData.pduData;

    float tR = APPLY_GAIN(gMetrologyWorkingData.phases[0].readings.RMSVoltage, cal->secondary.voltage.R);
    float tY = APPLY_GAIN(gMetrologyWorkingData.phases[1].readings.RMSVoltage, cal->secondary.voltage.Y);
    float tB = APPLY_GAIN(gMetrologyWorkingData.phases[2].readings.RMSVoltage, cal->secondary.voltage.B);

    *vRY = sqrtf(tR * tR + tY * tY + tR * tY);
    *vYB = sqrtf(tY * tY + tB * tB + tY * tB);
    *vBR = sqrtf(tB * tB + tR * tR + tB * tR);

}

/* ============================================================
 *  1. VOLTAGE LACK DETECTION
 *
 *  Compares each pair of L-L voltages against each other.
 *  If any pair differs by more than VOLT_LACK_THRESHOLD -> fault.
 *  Clears with hysteresis.
 * ============================================================ */
void PhaseDetection_voltageLack(void)
{
    float vRY, vYB, vBR;
    float diff_RY, diff_YB, diff_BR;
    float clear = VOLT_LACK_THRESHOLD - VOLT_LACK_HYSTERESIS;

    /* --- Primary (Input) --- */
    getPrimaryLL(&vRY, &vYB, &vBR);
    diff_RY = fabsval(vRY - vYB);
    diff_YB = fabsval(vYB - vBR);
    diff_BR = fabsval(vBR - vRY);

    if ((diff_RY > VOLT_LACK_THRESHOLD) ||
        (diff_YB > VOLT_LACK_THRESHOLD) ||
        (diff_BR > VOLT_LACK_THRESHOLD))
    {
        sBoardRuntimeData.inputParams.voltageLack = 1;
    }
    else if ((diff_RY < clear) && (diff_YB < clear) && (diff_BR < clear))
    {
        sBoardRuntimeData.inputParams.voltageLack = 0;
    }

    /* --- Secondary (Output) --- */
    getSecondaryLL(&vRY, &vYB, &vBR);
    diff_RY = fabsval(vRY - vYB);
    diff_YB = fabsval(vYB - vBR);
    diff_BR = fabsval(vBR - vRY);

    if ((diff_RY > VOLT_LACK_THRESHOLD) ||
        (diff_YB > VOLT_LACK_THRESHOLD) ||
        (diff_BR > VOLT_LACK_THRESHOLD))
    {
        sBoardRuntimeData.outputParams.voltageLack = 1;
    }
    else if ((diff_RY < clear) && (diff_YB < clear) && (diff_BR < clear))
    {
        sBoardRuntimeData.outputParams.voltageLack = 0;
    }
}

/* ============================================================
 *  2. VOLTAGE UNBALANCE DETECTION
 *
 *  Checks if any L-L voltage deviates from the average
 *  by more than VOLT_UNBALANCE_PERCENT (%).
 *  Formula: |V_phase - V_avg| * 100 > V_avg * threshold
 * ============================================================ */
void PhaseDetection_voltageUnbalance(void)
{
    float vRY, vYB, vBR, avg;
    float dev_RY, dev_YB, dev_BR;
    float limit;

    /* --- Primary (Input) --- */
    getPrimaryLL(&vRY, &vYB, &vBR);
    avg = (vRY + vYB + vBR) / 3.0f;

    if (avg > 1.0f)
    {
        dev_RY = fabsval(vRY - avg);
        dev_YB = fabsval(vYB - avg);
        dev_BR = fabsval(vBR - avg);
        limit  = avg * VOLT_UNBALANCE_PERCENT;

        if ((dev_RY * 100.0f > limit) ||
            (dev_YB * 100.0f > limit) ||
            (dev_BR * 100.0f > limit))
        {
            sBoardRuntimeData.inputParams.voltageUnbalance = 1;
        }
        else
        {
            sBoardRuntimeData.inputParams.voltageUnbalance = 0;
        }
    }
    else
    {
        sBoardRuntimeData.inputParams.voltageUnbalance = 0;
    }

    /* --- Secondary (Output) --- */
    getSecondaryLL(&vRY, &vYB, &vBR);
    avg = (vRY + vYB + vBR) / 3.0f;

    if (avg > 1.0f)
    {
        dev_RY = fabsval(vRY - avg);
        dev_YB = fabsval(vYB - avg);
        dev_BR = fabsval(vBR - avg);
        limit  = avg * VOLT_UNBALANCE_PERCENT;

        if ((dev_RY * 100.0f > limit) ||
            (dev_YB * 100.0f > limit) ||
            (dev_BR * 100.0f > limit))
        {
            sBoardRuntimeData.outputParams.voltageUnbalance = 1;
        }
        else
        {
            sBoardRuntimeData.outputParams.voltageUnbalance = 0;
        }
    }
    else
    {
        sBoardRuntimeData.outputParams.voltageUnbalance = 0;
    }
}

/* ============================================================
 *  3. PHASE LOSS DETECTION
 *
 *  If any L-L voltage drops below PHASE_LOSS_THRESHOLD -> fault.
 *  Clears when ALL voltages recover above
 *  (PHASE_LOSS_THRESHOLD + PHASE_LOSS_HYSTERESIS).
 * ============================================================ */
void PhaseDetection_phaseLoss(void)
{

    float clear = PHASE_LOSS_THRESHOLD + PHASE_LOSS_HYSTERESIS;

    /* --- Primary (Input) --- */
    getPrimaryLL(&vRY, &vYB, &vBR);

    if ((vRY < PHASE_LOSS_THRESHOLD) ||
        (vYB < PHASE_LOSS_THRESHOLD) ||
        (vBR < PHASE_LOSS_THRESHOLD))
    {
        sBoardRuntimeData.inputParams.phaseLoss = 1;
    }
    else if ((vRY > clear) && (vYB > clear) && (vBR > clear))
    {
        sBoardRuntimeData.inputParams.phaseLoss = 0;
    }

    /* --- Secondary (Output) --- */
    getSecondaryLL(&vRY, &vYB, &vBR);

    if ((vRY < PHASE_LOSS_THRESHOLD) ||
        (vYB < PHASE_LOSS_THRESHOLD) ||
        (vBR < PHASE_LOSS_THRESHOLD))
    {
        sBoardRuntimeData.outputParams.phaseLoss = 1;
    }
    else if ((vRY > clear) && (vYB > clear) && (vBR > clear))
    {
        sBoardRuntimeData.outputParams.phaseLoss = 0;
    }
}

/* ============================================================
 *  4. PHASE SEQUENCE ERROR DETECTION
 *
 *  Method: Clarke Transform + Cross Product
 *
 *  From instantaneous R, Y, B samples, compute:
 *    v_alpha = (2/3)*R - (1/3)*Y - (1/3)*B
 *    v_beta  = (1/sqrt3)*(Y - B)
 *
 *  Then check rotation direction of the (alpha, beta) vector:
 *    cross = prev_alpha * curr_beta - prev_beta * curr_alpha
 *    cross > 0  ->  counter-clockwise  ->  healthy (R->Y->B)
 *    cross < 0  ->  clockwise  ->  reversed (R->B->Y)
 *
 *  Debounced with a vote counter to avoid noise glitches.
 *
 *  CALL THIS FROM THE ADC ISR with offset-corrected samples.
 * ============================================================ */
void PhaseDetection_phaseSequenceISR(int sample_R, int sample_Y, int sample_B)
{


    /* Clarke transform */
    v_alpha = (2.0f / 3.0f) * (float)sample_R
            - (1.0f / 3.0f) * (float)sample_Y
            - (1.0f / 3.0f) * (float)sample_B;

    v_beta  = (1.0f / 1.7320508f) * ((float)sample_Y - (float)sample_B);
    /* 1.7320508 = sqrt(3) */

    /* Cross product to determine rotation direction */
    cross = prev_v_alpha * v_beta - prev_v_beta * v_alpha;

    /* Store for next call */
    prev_v_alpha = v_alpha;
    prev_v_beta  = v_beta;

    /* Skip if signal too small (no meaningful rotation) */
    if ((fabsval(v_alpha) < 30.0f) && (fabsval(v_beta) < 30.0f))
    {
        return;
    }

    /* Vote: positive = healthy, negative = reversed */
    if (cross > 0.0f)
    {
        if (seq_vote_counter < PHASE_SEQ_VOTE_THRESHOLD)
            seq_vote_counter++;
    }
    else if (cross < 0.0f)
    {
        if (seq_vote_counter > -PHASE_SEQ_VOTE_THRESHOLD)
            seq_vote_counter--;
    }

    /* Update status when vote counter reaches threshold */
    if (seq_vote_counter >= PHASE_SEQ_VOTE_THRESHOLD)
    {
        phase_seq_status = 0;  /* Healthy: R->Y->B */
    }
    else if (seq_vote_counter <= -PHASE_SEQ_VOTE_THRESHOLD)
    {
        phase_seq_status = 1;  /* Reversed: R->B->Y */
    }

    /* Write to both input and output runtime data */
    sBoardRuntimeData.inputParams.phaseSequenceError  = phase_seq_status;
    sBoardRuntimeData.outputParams.phaseSequenceError = phase_seq_status;
}

/* ============================================================
 *  5. CREST FACTOR CALCULATION (float32_t version)
 *
 *  Crest Factor = Peak / RMS (from float ADC samples)
 *  Auto-removes DC offset (mean).
 *  Pure sine wave = ~1.414 (sqrt2)
 *  Higher values indicate distorted/peaky waveforms.
 * ============================================================ */
float PhaseDetection_crestFactorF(const float32_t samples[], int num_samples)
{
    if (num_samples == 0)
    {
        return 0.0f;
    }

    int i;
    float sum = 0.0f;

    /* Pass 1: Compute DC offset (mean of all samples) */
    for (i = 0; i < num_samples; i++)
    {
        sum += samples[i];
    }
    float dc_offset = sum / num_samples;

    /* Pass 2: Find peak and sum of squares of AC component */
    float peak_value = 0.0f;
    float sum_of_squares = 0.0f;

    for (i = 0; i < num_samples; i++)
    {
        float ac_sample = samples[i] - dc_offset;
        float abs_val = fabsval(ac_sample);
        if (abs_val > peak_value)
        {
            peak_value = abs_val;
        }
        sum_of_squares += ac_sample * ac_sample;
    }

    float rms_value = sqrtf(sum_of_squares / num_samples);

    if (rms_value < FLT_MIN)
    {
        return 0.0f;
    }

    return peak_value / rms_value;
}

/* ============================================================
 *  5b. COMPUTE CREST FACTORS FOR INPUT AND OUTPUT VOLTAGES
 *
 *  Uses THD raw sample buffers (g_adcState.rawBuffers):
 *    Input:  channels 6,7,8  (THD_CH_IN_VOLT_R/Y/B)
 *    Output: channels 0,1,2  (THD_CH_OUT_VOLT_R/Y/B)
 *
 *  Scaling: x100  (1.414 -> 141)
 *
 *  Call AFTER THD buffer is ready.
 * ============================================================ */
void PhaseDetection_computeCrestFactors(void)
{
    /* Input (primary) crest factors */
    float cfR = PhaseDetection_crestFactorF(g_adcState.rawBuffers[THD_CH_IN_VOLT_R], THD_FFT_SIZE);
    float cfY = PhaseDetection_crestFactorF(g_adcState.rawBuffers[THD_CH_IN_VOLT_Y], THD_FFT_SIZE);
    float cfB = PhaseDetection_crestFactorF(g_adcState.rawBuffers[THD_CH_IN_VOLT_B], THD_FFT_SIZE);

    sBoardRuntimeData.inputParams.inputCrestFactorRP = (uint16_t)(cfR * 100.0f);
    sBoardRuntimeData.inputParams.inputCrestFactorYP = (uint16_t)(cfY * 100.0f);
    sBoardRuntimeData.inputParams.inputCrestFactorBP = (uint16_t)(cfB * 100.0f);

    /* Output (secondary) crest factors */
    cfR = PhaseDetection_crestFactorF(g_adcState.rawBuffers[THD_CH_OUT_VOLT_R], THD_FFT_SIZE);
    cfY = PhaseDetection_crestFactorF(g_adcState.rawBuffers[THD_CH_OUT_VOLT_Y], THD_FFT_SIZE);
    cfB = PhaseDetection_crestFactorF(g_adcState.rawBuffers[THD_CH_OUT_VOLT_B], THD_FFT_SIZE);

    sBoardRuntimeData.outputParams.outputCrestFactorRP = (uint16_t)(cfR * 100.0f);
    sBoardRuntimeData.outputParams.outputCrestFactorYP = (uint16_t)(cfY * 100.0f);
    sBoardRuntimeData.outputParams.outputCrestFactorBP = (uint16_t)(cfB * 100.0f);
}

/* ============================================================
 *  INIT
 * ============================================================ */
void PhaseDetection_init(void)
{
    prev_v_alpha = 0.0f;
    prev_v_beta  = 0.0f;
    seq_vote_counter = 0;
    phase_seq_status = 0;

    sBoardRuntimeData.inputParams.voltageLack         = 0;
    sBoardRuntimeData.inputParams.voltageUnbalance    = 0;
    sBoardRuntimeData.inputParams.phaseLoss           = 0;
    sBoardRuntimeData.inputParams.phaseSequenceError  = 0;

    sBoardRuntimeData.outputParams.voltageLack        = 0;
    sBoardRuntimeData.outputParams.voltageUnbalance   = 0;
    sBoardRuntimeData.outputParams.phaseLoss          = 0;
    sBoardRuntimeData.outputParams.phaseSequenceError = 0;
}

/* ============================================================
 *  RUN ALL (main loop convenience function)
 *  Calls voltage lack + voltage unbalance + phase loss.
 *  Phase sequence is handled separately in ISR.
 * ============================================================ */
void PhaseDetection_runAll(void)
{
    PhaseDetection_voltageLack();
    PhaseDetection_voltageUnbalance();
    PhaseDetection_phaseLoss();
}
