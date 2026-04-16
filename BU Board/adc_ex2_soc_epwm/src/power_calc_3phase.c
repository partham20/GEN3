/*
 * power_calc_3phase.c
 *
 * Three-Phase Power Calculation Module Implementation
 *
 * Calculates Real Power for all 18 CTs:
 *   P_real[ct] = (1/N) * SUM(V_phase[i] * I_ct[i]) for i = 0 to N-1
 *
 * CT to Voltage Phase Mapping:
 *   CT1-CT6   (R-Phase CTs) use R-Phase voltage
 *   CT7-CT12  (Y-Phase CTs) use Y-Phase voltage
 *   CT13-CT18 (B-Phase CTs) use B-Phase voltage
 */

#include "power_calc_3phase.h"
#include "ct_dma.h"
#include "voltage_rx.h"
#include "metro.h"          /* For gMetrologyWorkingData - IRMS access */
#include "calibration.h"    /* For g_calibration.currentGain - runtime calibration */
#include <string.h>
#include <math.h>           /* For sqrtf() */

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/*
 * Ping-Pong Buffers for 3-Phase Voltage (from CAN)
 * Place in RAM sections for fast access
 */
#pragma DATA_SECTION(g_voltageRPhase_A, "ramgs5")
#pragma DATA_SECTION(g_voltageRPhase_B, "ramgs5")
volatile int16_t g_voltageRPhase_A[POWER_3P_BUFFER_SIZE];
volatile int16_t g_voltageRPhase_B[POWER_3P_BUFFER_SIZE];

#pragma DATA_SECTION(g_voltageYPhase_A, "ramgs6")
#pragma DATA_SECTION(g_voltageYPhase_B, "ramgs6")
volatile int16_t g_voltageYPhase_A[POWER_3P_BUFFER_SIZE];
volatile int16_t g_voltageYPhase_B[POWER_3P_BUFFER_SIZE];

#pragma DATA_SECTION(g_voltageBPhase_A, "ramgs7")
#pragma DATA_SECTION(g_voltageBPhase_B, "ramgs7")
volatile int16_t g_voltageBPhase_A[POWER_3P_BUFFER_SIZE];
volatile int16_t g_voltageBPhase_B[POWER_3P_BUFFER_SIZE];

/* Active buffer indicators (0 = Buffer A active, 1 = Buffer B active) */
volatile uint16_t g_activeVoltageR = 0U;
volatile uint16_t g_activeVoltageY = 0U;
volatile uint16_t g_activeVoltageB = 0U;

/* Power calculation results */
volatile Power3Phase_Results g_power3PhaseResults;

/* Power calculation status */
volatile Power3Phase_Status g_power3PhaseStatus = {
    .isReady = false,
    .isValid = false,
    .sampleCount = 0U,
    .cycleCount = 0U
};

/* Static filtered power values for each CT (persist between calls) */
static float s_ctFilteredPower[POWER_TOTAL_CTS] = {0};

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/**
 * @brief   Initialize the 3-phase power calculation module
 */
void Power3Phase_init(void)
{
    uint32_t i;
    uint16_t ct;

    /* Clear R-phase voltage buffers */
    for(i = 0U; i < POWER_3P_BUFFER_SIZE; i++)
    {
        g_voltageRPhase_A[i] = 0;
        g_voltageRPhase_B[i] = 0;
    }

    /* Clear Y-phase voltage buffers */
    for(i = 0U; i < POWER_3P_BUFFER_SIZE; i++)
    {
        g_voltageYPhase_A[i] = 0;
        g_voltageYPhase_B[i] = 0;
    }

    /* Clear B-phase voltage buffers */
    for(i = 0U; i < POWER_3P_BUFFER_SIZE; i++)
    {
        g_voltageBPhase_A[i] = 0;
        g_voltageBPhase_B[i] = 0;
    }

    /* Reset active buffer indicators */
    g_activeVoltageR = 0U;
    g_activeVoltageY = 0U;
    g_activeVoltageB = 0U;

    /* Clear filtered power history */
    for(ct = 0U; ct < POWER_TOTAL_CTS; ct++)
    {
        s_ctFilteredPower[ct] = 0.0f;
    }

    /* Clear results */
    memset((void*)&g_power3PhaseResults, 0, sizeof(Power3Phase_Results));

    /* Reset status */
    g_power3PhaseStatus.isReady = false;
    g_power3PhaseStatus.isValid = false;
    g_power3PhaseStatus.sampleCount = 0U;
    g_power3PhaseStatus.cycleCount = 0U;
}

/**
 * @brief   Reset the 3-phase power calculation module
 */
void Power3Phase_reset(void)
{
    g_power3PhaseStatus.isReady = false;
    g_power3PhaseStatus.isValid = false;
}

/* ======================== Voltage Buffer Functions ======================== */

/**
 * @brief   Get pointer to active voltage buffer for a phase
 */
volatile int16_t* Power3Phase_getActiveVoltageBuffer(uint16_t phase)
{
    switch(phase)
    {
        case POWER_PHASE_R:
            return (g_activeVoltageR == 0U) ? g_voltageRPhase_A : g_voltageRPhase_B;

        case POWER_PHASE_Y:
            return (g_activeVoltageY == 0U) ? g_voltageYPhase_A : g_voltageYPhase_B;

        case POWER_PHASE_B:
            return (g_activeVoltageB == 0U) ? g_voltageBPhase_A : g_voltageBPhase_B;

        default:
            return NULL;
    }
}

/**
 * @brief   Get pointer to inactive voltage buffer for a phase
 */
volatile int16_t* Power3Phase_getInactiveVoltageBuffer(uint16_t phase)
{
    switch(phase)
    {
        case POWER_PHASE_R:
            return (g_activeVoltageR == 0U) ? g_voltageRPhase_B : g_voltageRPhase_A;

        case POWER_PHASE_Y:
            return (g_activeVoltageY == 0U) ? g_voltageYPhase_B : g_voltageYPhase_A;

        case POWER_PHASE_B:
            return (g_activeVoltageB == 0U) ? g_voltageBPhase_B : g_voltageBPhase_A;

        default:
            return NULL;
    }
}

/**
 * @brief   Swap voltage buffer for a specific phase
 */
void Power3Phase_swapVoltageBuffer(uint16_t phase)
{
    switch(phase)
    {
        case POWER_PHASE_R:
            g_activeVoltageR = (g_activeVoltageR == 0U) ? 1U : 0U;
            break;

        case POWER_PHASE_Y:
            g_activeVoltageY = (g_activeVoltageY == 0U) ? 1U : 0U;
            break;

        case POWER_PHASE_B:
            g_activeVoltageB = (g_activeVoltageB == 0U) ? 1U : 0U;
            break;

        default:
            break;
    }
}

/**
 * @brief   Swap voltage buffers for all 3 phases
 */
void Power3Phase_swapAllVoltageBuffers(void)
{
    g_activeVoltageR = (g_activeVoltageR == 0U) ? 1U : 0U;
    g_activeVoltageY = (g_activeVoltageY == 0U) ? 1U : 0U;
    g_activeVoltageB = (g_activeVoltageB == 0U) ? 1U : 0U;
}

/**
 * @brief   Copy voltage data from source buffer to active ping-pong buffer
 */
void Power3Phase_copyVoltageToActive(uint16_t phase, const volatile int16_t* srcBuffer, uint16_t count)
{
    volatile int16_t* destBuffer;
    uint32_t i;
    uint16_t copyCount;

    destBuffer = Power3Phase_getActiveVoltageBuffer(phase);
    if(destBuffer == NULL || srcBuffer == NULL)
    {
        return;
    }

    copyCount = (count > POWER_3P_BUFFER_SIZE) ? POWER_3P_BUFFER_SIZE : count;

    for(i = 0U; i < copyCount; i++)
    {
        destBuffer[i] = srcBuffer[i];
    }
}

/* ======================== Power Calculation Functions ======================== */

/**
 * @brief   Calculate power for a single CT with filtering
 *          P_real = (1/N) * SUM(V[i] * I[i])
 */
static int32_t calculateCTPowerInternal(uint16_t ctIndex,
                                        const volatile int16_t* voltageBuffer,
                                        const volatile int16_t* currentBuffer,
                                        int64_t* pAccumulator)
{
    int64_t accumulator = 0;
    int32_t product;
    uint32_t i;
    int32_t instantaneousPower;
    float filteredPower;

    /* Calculate sum of instantaneous power: SUM(V[i] * I[i]) */
    for(i = 0U; i < POWER_3P_BUFFER_SIZE; i++)
    {
        product = (int32_t)voltageBuffer[i] * (int32_t)currentBuffer[i];
        accumulator += product;
    }

    /* Store accumulator for debug */
    if(pAccumulator != NULL)
    {
        *pAccumulator = accumulator;
    }

    /* Calculate average (instantaneous) power */
    instantaneousPower = (int32_t)(accumulator / POWER_3P_BUFFER_SIZE);

    /* Apply low-pass filter */
    if(g_power3PhaseStatus.cycleCount == 0U)
    {
        /* First cycle - initialize filter */
        s_ctFilteredPower[ctIndex] = (float)instantaneousPower;
    }
    else
    {
        /* Apply exponential moving average filter */
        filteredPower = (POWER_FILTER_ALPHA * (float)instantaneousPower) +
                        ((1.0f - POWER_FILTER_ALPHA) * s_ctFilteredPower[ctIndex]);
        s_ctFilteredPower[ctIndex] = filteredPower;
    }

    return (int32_t)s_ctFilteredPower[ctIndex];
}

/**
 * @brief   Calculate power for all 18 CTs
 *          Uses inactive voltage and current buffers
 */
#pragma CODE_SECTION(Power3Phase_calculateAllPower, ".TI.ramfunc")
void Power3Phase_calculateAllPower(void)
{
    uint16_t ct;
    uint16_t ctIndex;
    uint16_t phaseCtIndex;
    volatile int16_t* voltageBuffer;
    volatile int16_t* currentBuffer;
    int32_t ctPower;
    int64_t accumulator;

    /* Phase totals */
    int32_t rPhaseTotal = 0;
    int32_t yPhaseTotal = 0;
    int32_t bPhaseTotal = 0;

    /* Get inactive voltage buffers for each phase */
    volatile int16_t* voltageR = Power3Phase_getInactiveVoltageBuffer(POWER_PHASE_R);
    volatile int16_t* voltageY = Power3Phase_getInactiveVoltageBuffer(POWER_PHASE_Y);
    volatile int16_t* voltageB = Power3Phase_getInactiveVoltageBuffer(POWER_PHASE_B);

    /*
     * ==================== R-Phase CTs (CT1-CT6) ====================
     * CT Index 0-5, use R-phase voltage
     */
    for(ct = 1U; ct <= 6U; ct++)
    {
        ctIndex = ct - 1U;
        phaseCtIndex = ctIndex;  /* 0-5 within R-phase */

        /* Get inactive current buffer for this CT */
        currentBuffer = CT_DMA_getInactiveCTBuffer(ct);
        voltageBuffer = voltageR;

        if(currentBuffer != NULL && voltageBuffer != NULL)
        {
            ctPower = calculateCTPowerInternal(ctIndex, voltageBuffer, currentBuffer, &accumulator);

            /* Store results */
            g_power3PhaseResults.rPhase.ct[phaseCtIndex].realPower_raw = ctPower;
            g_power3PhaseResults.rPhase.ct[phaseCtIndex].realPower_watts = (float)ctPower * POWER_SCALE_3P;
            g_power3PhaseResults.rPhase.ct[phaseCtIndex].filteredPower = s_ctFilteredPower[ctIndex];
            g_power3PhaseResults.rPhase.ct[phaseCtIndex].accumulator = accumulator;

            rPhaseTotal += ctPower;
        }
    }

    /* Store R-phase totals */
    g_power3PhaseResults.rPhase.totalPower_raw = rPhaseTotal;
    g_power3PhaseResults.rPhase.totalPower_watts = (float)rPhaseTotal * POWER_SCALE_3P;

    /*
     * ==================== Y-Phase CTs (CT7-CT12) ====================
     * CT Index 6-11, use Y-phase voltage
     */
    for(ct = 7U; ct <= 12U; ct++)
    {
        ctIndex = ct - 1U;
        phaseCtIndex = ctIndex - 6U;  /* 0-5 within Y-phase */

        /* Get inactive current buffer for this CT */
        currentBuffer = CT_DMA_getInactiveCTBuffer(ct);
        voltageBuffer = voltageY;

        if(currentBuffer != NULL && voltageBuffer != NULL)
        {
            ctPower = calculateCTPowerInternal(ctIndex, voltageBuffer, currentBuffer, &accumulator);

            /* Store results */
            g_power3PhaseResults.yPhase.ct[phaseCtIndex].realPower_raw = ctPower;
            g_power3PhaseResults.yPhase.ct[phaseCtIndex].realPower_watts = (float)ctPower * POWER_SCALE_3P;
            g_power3PhaseResults.yPhase.ct[phaseCtIndex].filteredPower = s_ctFilteredPower[ctIndex];
            g_power3PhaseResults.yPhase.ct[phaseCtIndex].accumulator = accumulator;

            yPhaseTotal += ctPower;
        }
    }

    /* Store Y-phase totals */
    g_power3PhaseResults.yPhase.totalPower_raw = yPhaseTotal;
    g_power3PhaseResults.yPhase.totalPower_watts = (float)yPhaseTotal * POWER_SCALE_3P;

    /*
     * ==================== B-Phase CTs (CT13-CT18) ====================
     * CT Index 12-17, use B-phase voltage
     */
    for(ct = 13U; ct <= 18U; ct++)
    {
        ctIndex = ct - 1U;
        phaseCtIndex = ctIndex - 12U;  /* 0-5 within B-phase */

        /* Get inactive current buffer for this CT */
        currentBuffer = CT_DMA_getInactiveCTBuffer(ct);
        voltageBuffer = voltageB;

        if(currentBuffer != NULL && voltageBuffer != NULL)
        {
            ctPower = calculateCTPowerInternal(ctIndex, voltageBuffer, currentBuffer, &accumulator);

            /* Store results */
            g_power3PhaseResults.bPhase.ct[phaseCtIndex].realPower_raw = ctPower;
            g_power3PhaseResults.bPhase.ct[phaseCtIndex].realPower_watts = (float)ctPower * POWER_SCALE_3P;
            g_power3PhaseResults.bPhase.ct[phaseCtIndex].filteredPower = s_ctFilteredPower[ctIndex];
            g_power3PhaseResults.bPhase.ct[phaseCtIndex].accumulator = accumulator;

            bPhaseTotal += ctPower;
        }
    }

    /* Store B-phase totals */
    g_power3PhaseResults.bPhase.totalPower_raw = bPhaseTotal;
    g_power3PhaseResults.bPhase.totalPower_watts = (float)bPhaseTotal * POWER_SCALE_3P;

    /* Calculate total system power */
    g_power3PhaseResults.totalSystem_raw = rPhaseTotal + yPhaseTotal + bPhaseTotal;
    g_power3PhaseResults.totalSystem_watts = (float)g_power3PhaseResults.totalSystem_raw * POWER_SCALE_3P;

    /* Update status */
    g_power3PhaseStatus.sampleCount = POWER_3P_BUFFER_SIZE;
    g_power3PhaseStatus.cycleCount++;
    g_power3PhaseStatus.isReady = true;
    g_power3PhaseStatus.isValid = true;
}

/**
 * @brief   Calculate power for a single CT (public API)
 */
int32_t Power3Phase_calculateCTPower(uint16_t ctNumber,
                                      const volatile int16_t* voltageBuffer,
                                      const volatile int16_t* currentBuffer)
{
    if(ctNumber < 1U || ctNumber > POWER_TOTAL_CTS)
    {
        return 0;
    }

    return calculateCTPowerInternal(ctNumber - 1U, voltageBuffer, currentBuffer, NULL);
}

/* ======================== Result Access Functions ======================== */

/**
 * @brief   Get power result for a specific CT
 */
const CT_PowerResult* Power3Phase_getCTResult(uint16_t ctNumber)
{
    if(ctNumber < 1U || ctNumber > POWER_TOTAL_CTS)
    {
        return NULL;
    }

    if(ctNumber <= 6U)
    {
        /* R-phase: CT1-CT6 */
        return (const CT_PowerResult*)&g_power3PhaseResults.rPhase.ct[ctNumber - 1U];
    }
    else if(ctNumber <= 12U)
    {
        /* Y-phase: CT7-CT12 */
        return (const CT_PowerResult*)&g_power3PhaseResults.yPhase.ct[ctNumber - 7U];
    }
    else
    {
        /* B-phase: CT13-CT18 */
        return (const CT_PowerResult*)&g_power3PhaseResults.bPhase.ct[ctNumber - 13U];
    }
}

/**
 * @brief   Get power result for a specific phase
 */
const Phase_PowerResult* Power3Phase_getPhaseResult(uint16_t phase)
{
    switch(phase)
    {
        case POWER_PHASE_R:
            return (const Phase_PowerResult*)&g_power3PhaseResults.rPhase;

        case POWER_PHASE_Y:
            return (const Phase_PowerResult*)&g_power3PhaseResults.yPhase;

        case POWER_PHASE_B:
            return (const Phase_PowerResult*)&g_power3PhaseResults.bPhase;

        default:
            return NULL;
    }
}

/**
 * @brief   Get total system power in raw units
 */
int32_t Power3Phase_getTotalPower_raw(void)
{
    return g_power3PhaseResults.totalSystem_raw;
}

/**
 * @brief   Get total system power in Watts
 */
float Power3Phase_getTotalPower_watts(void)
{
    return g_power3PhaseResults.totalSystem_watts;
}

/**
 * @brief   Get power for a specific CT in raw units
 */
int32_t Power3Phase_getCTPower_raw(uint16_t ctNumber)
{
    const CT_PowerResult* result = Power3Phase_getCTResult(ctNumber);
    return (result != NULL) ? result->realPower_raw : 0;
}

/**
 * @brief   Get power for a specific CT in Watts
 */
float Power3Phase_getCTPower_watts(uint16_t ctNumber)
{
    const CT_PowerResult* result = Power3Phase_getCTResult(ctNumber);
    return (result != NULL) ? result->realPower_watts : 0.0f;
}

/**
 * @brief   Get total phase power in raw units
 */
int32_t Power3Phase_getPhaseTotalPower_raw(uint16_t phase)
{
    const Phase_PowerResult* result = Power3Phase_getPhaseResult(phase);
    return (result != NULL) ? result->totalPower_raw : 0;
}

/**
 * @brief   Get total phase power in Watts
 */
float Power3Phase_getPhaseTotalPower_watts(uint16_t phase)
{
    const Phase_PowerResult* result = Power3Phase_getPhaseResult(phase);
    return (result != NULL) ? result->totalPower_watts : 0.0f;
}

/**
 * @brief   Check if power calculation is ready
 */
bool Power3Phase_isReady(void)
{
    return g_power3PhaseStatus.isReady;
}

/**
 * @brief   Get calculation cycle count
 */
uint32_t Power3Phase_getCycleCount(void)
{
    return g_power3PhaseStatus.cycleCount;
}

/**
 * @brief   Extract VRMS from CAN frame bytes 62-63 for a specific phase
 *          CAN uses 8-bit bytes, MCU uses 16-bit minimum addressable unit
 *          Data format: Big-endian (high byte at index 62, low byte at index 63)
 *
 * @param   frameData - Pointer to CAN frame data (uint16_t array from MCAN_RxBufElement.data)
 * @param   phaseId - Phase identifier (PHASE_ID_R=1, PHASE_ID_Y=2, PHASE_ID_B=3)
 */
void Power3Phase_extractVrmsFromCAN(const uint16_t* frameData, uint8_t phaseId)
{
    uint8_t *byteData;
    uint8_t highByte;
    uint8_t lowByte;
    uint16_t vrmsRaw;
    float vrmsValue;

    if(frameData == NULL)
    {
        return;
    }

    /* Cast uint16_t* to uint8_t* for 8-bit byte access
     * MCU has 16-bit minimum addressable unit, but CAN data is 8-bit bytes
     * The MCAN driver stores CAN bytes packed into uint16_t array */
    byteData = (uint8_t *)frameData;

    /* Extract bytes 62 and 63 (0-indexed) - last 2 bytes of 64-byte CAN frame
     * Big-endian format: byte 62 is high byte, byte 63 is low byte */
    highByte = byteData[62];
    lowByte = byteData[63];

    /* Combine bytes to form 16-bit VRMS value */
    vrmsRaw = ((uint16_t)highByte << 8U) | (uint16_t)lowByte;

    /* Convert to voltage: divide by 100 to get values like 230.xx
     * (e.g., CAN sends 23050 -> VRMS = 230.50V) */
    vrmsValue = (float)vrmsRaw / 100.0f;

    /* Store in the appropriate phase VRMS field */
    switch(phaseId)
    {
        case 1U:  /* PHASE_ID_R */
            g_power3PhaseResults.vrmsR = vrmsValue;
            break;

        case 2U:  /* PHASE_ID_Y */
            g_power3PhaseResults.vrmsY = vrmsValue;
            break;

        case 3U:  /* PHASE_ID_B */
            g_power3PhaseResults.vrmsB = vrmsValue;
            break;

        default:
            /* Unknown phase, ignore */
            break;
    }
}

/**
 * @brief   Calculate apparent power, reactive power and power factor for all 18 CTs
 *
 * Uses:
 *   - VRMS from g_power3PhaseResults.vrmsR/vrmsY/vrmsB (extracted from CAN)
 *   - IRMS from gMetrologyWorkingData.phases[ctIndex].readings.RMSCurrent
 *     where ctIndex 0 = CT1, ctIndex 1 = CT2, etc.
 *
 * Calculates:
 *   S (Apparent Power) = Vrms * Irms  [VA]
 *   Q (Reactive Power) = sqrt(S^2 - P^2)  [VAr]
 *   PF (Power Factor) = P / S  [unitless]
 *
 * Phase to VRMS mapping:
 *   R-Phase CTs (CT1-CT6)   use vrmsR
 *   Y-Phase CTs (CT7-CT12)  use vrmsY
 *   B-Phase CTs (CT13-CT18) use vrmsB
 *
 * Note: Call this AFTER Power3Phase_calculateAllPower() and App_calculateMetrologyParameters()
 */
#pragma CODE_SECTION(Power3Phase_calculateApparentReactivePF, ".TI.ramfunc")
void Power3Phase_calculateApparentReactivePF(void)
{
    uint16_t ct;
    uint16_t ctIndex;
    uint16_t phaseCtIndex;
    float vrmsR;
    float vrmsY;
    float vrmsB;
    float irms;
    float realPower;
    float apparentPower;
    float reactivePower;
    float powerFactor;
    float sSquared;
    float pSquared;
    float qSquared;

    /* Phase totals */
    float rPhaseApparent = 0.0f;
    float rPhaseReactive = 0.0f;
    float yPhaseApparent = 0.0f;
    float yPhaseReactive = 0.0f;
    float bPhaseApparent = 0.0f;
    float bPhaseReactive = 0.0f;

    /* Get VRMS for each phase from CAN data */
    vrmsR = g_power3PhaseResults.vrmsR;
    vrmsY = g_power3PhaseResults.vrmsY;
    vrmsB = g_power3PhaseResults.vrmsB;

    /* Skip calculation if all VRMS values are zero or invalid */
    if((vrmsR < 1.0f) && (vrmsY < 1.0f) && (vrmsB < 1.0f))
    {
        return;
    }

    /*
     * ==================== R-Phase CTs (CT1-CT6) ====================
     * CT Index 0-5, metrology phases[0-5], uses vrmsR
     */
    if(vrmsR >= 1.0f)
    {
        for(ct = 1U; ct <= 6U; ct++)
        {
            ctIndex = ct - 1U;
            phaseCtIndex = ctIndex;  /* 0-5 within R-phase */

            /* Get IRMS from metrology working data, apply runtime calibration gain
             * gMetrologyWorkingData.phases[ctIndex] where index 0 = CT1 */
            irms = gMetrologyWorkingData.phases[ctIndex].readings.RMSCurrent
                   * g_calibration.currentGain[ctIndex];

            /* Get real power already calculated */
            realPower = g_power3PhaseResults.rPhase.ct[phaseCtIndex].realPower_watts;

            /* Calculate Apparent Power: S = Vrms * Irms (using R-phase voltage) */
            apparentPower = vrmsR * irms;

            /* Calculate Reactive Power: Q = sqrt(S^2 - P^2)
             * Handle case where P > S (due to measurement errors) */
            sSquared = apparentPower * apparentPower;
            pSquared = realPower * realPower;
            qSquared = fabs(sSquared - pSquared);
            reactivePower = sqrtf(qSquared);
//            if(qSquared > 0.0f)
//            {
//                reactivePower = sqrtf(qSquared);
//            }
//            else
//            {
//                reactivePower = 0.0f;
//            }

            /* Calculate Power Factor: PF = P / S
             * Handle divide by zero */
            if(apparentPower > 0.001f)
            {
                powerFactor = realPower / apparentPower;
                /* Clamp power factor to valid range [-1, 1] */
                if(powerFactor > 1.0f) powerFactor = 1.0f;
                if(powerFactor < -1.0f) powerFactor = -1.0f;
            }
            else
            {
                powerFactor = 0.0f;
            }

            /* Store results */
            g_power3PhaseResults.rPhase.ct[phaseCtIndex].apparentPower_VA = apparentPower;
            g_power3PhaseResults.rPhase.ct[phaseCtIndex].reactivePower_VAr = reactivePower;
            g_power3PhaseResults.rPhase.ct[phaseCtIndex].powerFactor = powerFactor;

            /* Accumulate phase totals */
            rPhaseApparent += apparentPower;
            rPhaseReactive += reactivePower;
        }
    }

    /* Store R-phase totals and calculate phase power factor */
    g_power3PhaseResults.rPhase.totalApparentPower_VA = rPhaseApparent;
    g_power3PhaseResults.rPhase.totalReactivePower_VAr = rPhaseReactive;
    if(rPhaseApparent > 0.001f)
    {
        g_power3PhaseResults.rPhase.phasePowerFactor =
            g_power3PhaseResults.rPhase.totalPower_watts / rPhaseApparent;
    }
    else
    {
        g_power3PhaseResults.rPhase.phasePowerFactor = 0.0f;
    }

    /*
     * ==================== Y-Phase CTs (CT7-CT12) ====================
     * CT Index 6-11, metrology phases[6-11], uses vrmsY
     */
    if(vrmsY >= 1.0f)
    {
        for(ct = 7U; ct <= 12U; ct++)
        {
            ctIndex = ct - 1U;
            phaseCtIndex = ctIndex - 6U;  /* 0-5 within Y-phase */

            /* Get IRMS from metrology working data, apply runtime calibration gain */
            irms = gMetrologyWorkingData.phases[ctIndex].readings.RMSCurrent
                   * g_calibration.currentGain[ctIndex];

            /* Get real power already calculated */
            realPower = g_power3PhaseResults.yPhase.ct[phaseCtIndex].realPower_watts;

            /* Calculate Apparent Power: S = Vrms * Irms (using Y-phase voltage) */
            apparentPower = vrmsY * irms;

            /* Calculate Reactive Power: Q = sqrt(S^2 - P^2) */
            sSquared = apparentPower * apparentPower;
            pSquared = realPower * realPower;
            qSquared = fabs(sSquared - pSquared);
            reactivePower = sqrtf(qSquared);
//            if(qSquared > 0.0f)
//            {
//                reactivePower = sqrtf(qSquared);
//            }
//            else
//            {
//                reactivePower = 0.0f;
//            }

            /* Calculate Power Factor: PF = P / S */
            if(apparentPower > 0.001f)
            {
                powerFactor = realPower / apparentPower;
                if(powerFactor > 1.0f) powerFactor = 1.0f;
                if(powerFactor < -1.0f) powerFactor = -1.0f;
            }
            else
            {
                powerFactor = 0.0f;
            }

            /* Store results */
            g_power3PhaseResults.yPhase.ct[phaseCtIndex].apparentPower_VA = apparentPower;
            g_power3PhaseResults.yPhase.ct[phaseCtIndex].reactivePower_VAr = reactivePower;
            g_power3PhaseResults.yPhase.ct[phaseCtIndex].powerFactor = powerFactor;

            /* Accumulate phase totals */
            yPhaseApparent += apparentPower;
            yPhaseReactive += reactivePower;
        }
    }

    /* Store Y-phase totals and calculate phase power factor */
    g_power3PhaseResults.yPhase.totalApparentPower_VA = yPhaseApparent;
    g_power3PhaseResults.yPhase.totalReactivePower_VAr = yPhaseReactive;
    if(yPhaseApparent > 0.001f)
    {
        g_power3PhaseResults.yPhase.phasePowerFactor =
            g_power3PhaseResults.yPhase.totalPower_watts / yPhaseApparent;
    }
    else
    {
        g_power3PhaseResults.yPhase.phasePowerFactor = 0.0f;
    }

    /*
     * ==================== B-Phase CTs (CT13-CT18) ====================
     * CT Index 12-17, metrology phases[12-17], uses vrmsB
     */
    if(vrmsB >= 1.0f)
    {
        for(ct = 13U; ct <= 18U; ct++)
        {
            ctIndex = ct - 1U;
            phaseCtIndex = ctIndex - 12U;  /* 0-5 within B-phase */

            /* Get IRMS from metrology working data, apply runtime calibration gain */
            irms = gMetrologyWorkingData.phases[ctIndex].readings.RMSCurrent
                   * g_calibration.currentGain[ctIndex];

            /* Get real power already calculated */
            realPower = g_power3PhaseResults.bPhase.ct[phaseCtIndex].realPower_watts;

            /* Calculate Apparent Power: S = Vrms * Irms (using B-phase voltage) */
            apparentPower = vrmsB * irms;

            /* Calculate Reactive Power: Q = sqrt(S^2 - P^2) */
            sSquared = apparentPower * apparentPower;
            pSquared = realPower * realPower;
            qSquared = fabs(sSquared - pSquared);
            reactivePower = sqrtf(qSquared);
//            if(qSquared > 0.0f)
//            {
//                reactivePower = sqrtf(qSquared);
//            }
//            else
//            {
//                reactivePower = 0.0f;
//            }

            /* Calculate Power Factor: PF = P / S */
            if(apparentPower > 0.001f)
            {
                powerFactor = realPower / apparentPower;
                if(powerFactor > 1.0f) powerFactor = 1.0f;
                if(powerFactor < -1.0f) powerFactor = -1.0f;
            }
            else
            {
                powerFactor = 0.0f;
            }

            /* Store results */
            g_power3PhaseResults.bPhase.ct[phaseCtIndex].apparentPower_VA = apparentPower;
            g_power3PhaseResults.bPhase.ct[phaseCtIndex].reactivePower_VAr = reactivePower;
            g_power3PhaseResults.bPhase.ct[phaseCtIndex].powerFactor = powerFactor;

            /* Accumulate phase totals */
            bPhaseApparent += apparentPower;
            bPhaseReactive += reactivePower;
        }
    }

    /* Store B-phase totals and calculate phase power factor */
    g_power3PhaseResults.bPhase.totalApparentPower_VA = bPhaseApparent;
    g_power3PhaseResults.bPhase.totalReactivePower_VAr = bPhaseReactive;
    if(bPhaseApparent > 0.001f)
    {
        g_power3PhaseResults.bPhase.phasePowerFactor =
            g_power3PhaseResults.bPhase.totalPower_watts / bPhaseApparent;
    }
    else
    {
        g_power3PhaseResults.bPhase.phasePowerFactor = 0.0f;
    }

    /* Calculate total system values */
    g_power3PhaseResults.totalApparent_VA = rPhaseApparent + yPhaseApparent + bPhaseApparent;
    g_power3PhaseResults.totalReactive_VAr = rPhaseReactive + yPhaseReactive + bPhaseReactive;

    /* Calculate system power factor */
    if(g_power3PhaseResults.totalApparent_VA > 0.001f)
    {
        g_power3PhaseResults.systemPowerFactor =
            g_power3PhaseResults.totalSystem_watts / g_power3PhaseResults.totalApparent_VA;
        /* Clamp to valid range */
        if(g_power3PhaseResults.systemPowerFactor > 1.0f)
        {
            g_power3PhaseResults.systemPowerFactor = 1.0f;
        }
        if(g_power3PhaseResults.systemPowerFactor < -1.0f)
        {
            g_power3PhaseResults.systemPowerFactor = -1.0f;
        }
    }
    else
    {
        g_power3PhaseResults.systemPowerFactor = 0.0f;
    }
}
