/*
 * power_calc.c
 *
 * Power Calculation Module Implementation
 * Implements ping-pong buffers and real power calculation
 *
 * Real Power = (1/N) * SUM(V[i] * I[i]) for i = 0 to N-1
 */

#include "power_calc.h"
#include <string.h>

/* ========================================================================== */
/*                            Global Variables                                */
/* ========================================================================== */

/* Ping-pong buffers for voltage (from CAN) - placed in RAM for fast access */
#pragma DATA_SECTION(g_voltageBuffer_A, "ramgs0")
#pragma DATA_SECTION(g_voltageBuffer_B, "ramgs0")
volatile int16_t g_voltageBuffer_A[POWER_BUFFER_SIZE];
volatile int16_t g_voltageBuffer_B[POWER_BUFFER_SIZE];

/* Ping-pong buffers for current (from ADC/DMA) - placed in RAM for fast access */
#pragma DATA_SECTION(g_currentBuffer_A, "ramgs1")
#pragma DATA_SECTION(g_currentBuffer_B, "ramgs1")
volatile int16_t g_currentBuffer_A[POWER_BUFFER_SIZE];
volatile int16_t g_currentBuffer_B[POWER_BUFFER_SIZE];

/* Active buffer indices - which buffer is currently being written */
volatile PingPongIndex g_activeVoltageBuffer = BUFFER_A;
volatile PingPongIndex g_activeCurrentBuffer = BUFFER_A;
volatile int16_t *voltageBuffer;
   volatile int16_t *currentBuffer;

/* Power calculation status */
volatile PowerCalc_Status g_powerStatus = {
    .isReady = false,
    .isValid = false,
    .sampleCount = 0U,
    .cycleCount = 0U
};

/* Power calculation results */
volatile PowerCalc_Results g_powerResults = {
    .realPower_raw = 0,
    .realPower_watts = 0.0f,
    .accumulator = 0
};

/* Flag indicating new power data is available */
volatile bool g_newPowerDataAvailable = false;

/* ========================================================================== */
/*                          Function Definitions                              */
/* ========================================================================== */

/**
 * @brief   Initialize the power calculation module
 */
void PowerCalc_init(void)
{
    uint32_t i;

    /* Clear voltage buffers */
    for (i = 0U; i < POWER_BUFFER_SIZE; i++)
    {
        g_voltageBuffer_A[i] = 0;
        g_voltageBuffer_B[i] = 0;
    }

    /* Clear current buffers */
    for (i = 0U; i < POWER_BUFFER_SIZE; i++)
    {
        g_currentBuffer_A[i] = 0;
        g_currentBuffer_B[i] = 0;
    }

    /* Reset buffer indices to start with buffer A */
    g_activeVoltageBuffer = BUFFER_A;
    g_activeCurrentBuffer = BUFFER_A;

    /* Reset status */
    g_powerStatus.isReady = false;
    g_powerStatus.isValid = false;
    g_powerStatus.sampleCount = 0U;
    g_powerStatus.cycleCount = 0U;

    /* Reset results */
    g_powerResults.realPower_raw = 0;
    g_powerResults.realPower_watts = 0.0f;
    g_powerResults.accumulator = 0;

    /* Clear data ready flag */
    g_newPowerDataAvailable = false;
}

/**
 * @brief   Reset the power calculation module
 */
void PowerCalc_reset(void)
{
    /* Reset status flags */
    g_powerStatus.isReady = false;
    g_powerStatus.isValid = false;

    /* Clear data ready flag */
    g_newPowerDataAvailable = false;
}

/**
 * @brief   Get pointer to the active voltage write buffer
 */
volatile int16_t* PowerCalc_getActiveVoltageBuffer(void)
{
    if (g_activeVoltageBuffer == BUFFER_A)
    {
        return g_voltageBuffer_A;
    }
    else
    {
        return g_voltageBuffer_B;
    }
}

/**
 * @brief   Get pointer to the inactive voltage buffer (for calculation)
 */
volatile int16_t* PowerCalc_getInactiveVoltageBuffer(void)
{
    if (g_activeVoltageBuffer == BUFFER_A)
    {
        return g_voltageBuffer_B;  /* A is active, B is inactive */
    }
    else
    {
        return g_voltageBuffer_A;  /* B is active, A is inactive */
    }
}

/**
 * @brief   Get pointer to the active current write buffer
 */
volatile int16_t* PowerCalc_getActiveCurrentBuffer(void)
{
    if (g_activeCurrentBuffer == BUFFER_A)
    {
        return g_currentBuffer_A;
    }
    else
    {
        return g_currentBuffer_B;
    }
}

/**
 * @brief   Get pointer to the inactive current buffer (for calculation)
 */
volatile int16_t* PowerCalc_getInactiveCurrentBuffer(void)
{
    if (g_activeCurrentBuffer == BUFFER_A)
    {
        return g_currentBuffer_B;  /* A is active, B is inactive */
    }
    else
    {
        return g_currentBuffer_A;  /* B is active, A is inactive */
    }
}

/**
 * @brief   Swap the active voltage buffer
 */
void PowerCalc_swapVoltageBuffer(void)
{
    if (g_activeVoltageBuffer == BUFFER_A)
    {
        g_activeVoltageBuffer = BUFFER_B;
    }
    else
    {
        g_activeVoltageBuffer = BUFFER_A;
    }
}

/**
 * @brief   Swap the active current buffer
 */
void PowerCalc_swapCurrentBuffer(void)
{
    if (g_activeCurrentBuffer == BUFFER_A)
    {
        g_activeCurrentBuffer = BUFFER_B;
    }
    else
    {
        g_activeCurrentBuffer = BUFFER_A;
    }
}

/**
 * @brief   Calculate real power from the inactive buffers
 *          P_real = (1/N) * SUM(V[i] * I[i])
 *
 * @note    Runs from RAM for speed
 */

//
// Defines for the filter strength
// 0.01 = Very strong smoothing (slow response, stable)
// 0.10 = Moderate smoothing (good balance)
// 0.50 = Weak smoothing (fast response, some jitter)
//

#pragma CODE_SECTION(PowerCalc_calculateRealPower, ".TI.ramfunc")
void PowerCalc_calculateRealPower(void)
{
    int64_t accumulator = 0;
    int32_t product;
    uint32_t i;
    int32_t instantaneousRawPower;

    // Static variable to store the history (persists between function calls)
    static float s_filteredPower = 0.0f;

    /* Get the inactive buffers (contain complete data from previous cycle) */
    voltageBuffer = PowerCalc_getInactiveVoltageBuffer();
    currentBuffer = PowerCalc_getInactiveCurrentBuffer();

    /*
     * Calculate sum of instantaneous power: SUM(V[i] * I[i])
     */
    for (i = 0U; i < POWER_BUFFER_SIZE; i++)
    {
        /* Calculate instantaneous power: V[i] * I[i] */
        product = (int32_t)voltageBuffer[i] * (int32_t)currentBuffer[i];

        /* Accumulate */
        accumulator += product;
    }

    /* Store raw accumulator for debugging */
    g_powerResults.accumulator = accumulator;

    /*
     * 1. Calculate Instantaneous Average
     */
    instantaneousRawPower = (int32_t)(accumulator / POWER_BUFFER_SIZE);

    /*
     * 2. Apply Low Pass Filter (Smoothing)
     * Formula: Filtered = (Alpha * New) + ((1 - Alpha) * Old)
     */

    // Check if this is the very first cycle to initialize the filter immediately
    if (g_powerStatus.cycleCount == 0)
    {
        s_filteredPower = (float)instantaneousRawPower;
    }
    else
    {
        s_filteredPower = (FILTER_ALPHA * (float)instantaneousRawPower) +
                          ((1.0f - FILTER_ALPHA) * s_filteredPower);
    }

    /*
     * 3. Store the FILTERED result into the global struct
     */
    g_powerResults.realPower_raw = (int32_t)s_filteredPower;

    /* Calculate real power in Watts (with scaling) based on the filtered raw value */
    g_powerResults.realPower_watts = (float)g_powerResults.realPower_raw * POWER_SCALE_FACTOR;

    /* Update status */
    g_powerStatus.sampleCount = POWER_BUFFER_SIZE;
    g_powerStatus.cycleCount++;
    g_powerStatus.isReady = true;
    g_powerStatus.isValid = true;

    /* Set data ready flag */
    g_newPowerDataAvailable = true;
}

/**
 * @brief   Get the calculated real power in raw ADC units
 */
int32_t PowerCalc_getRealPower_raw(void)
{
    return g_powerResults.realPower_raw;
}

/**
 * @brief   Get the calculated real power in Watts
 */
float PowerCalc_getRealPower_watts(void)
{
    return g_powerResults.realPower_watts;
}

/**
 * @brief   Check if new power calculation data is available
 */
bool PowerCalc_isDataReady(void)
{
    return g_newPowerDataAvailable;
}

/**
 * @brief   Clear the data ready flag
 */
void PowerCalc_clearDataReady(void)
{
    g_newPowerDataAvailable = false;
}

/**
 * @brief   Copy ADC buffer to active current ping-pong buffer with offset removal
 *          Converts uint16_t ADC values (0-4095) to int16_t (-2048 to +2047)
 *
 * @note    Runs from RAM for speed
 */
#pragma CODE_SECTION(PowerCalc_copyCurrentFromADC, ".TI.ramfunc")
void PowerCalc_copyCurrentFromADC(const uint16_t *adcBuffer, uint16_t count)
{
    volatile int16_t *destBuffer;
    uint32_t i;
    uint16_t copyCount;

    /* Get active current buffer */
    destBuffer = PowerCalc_getActiveCurrentBuffer();

    /* Limit copy count to buffer size */
    copyCount = (count > POWER_BUFFER_SIZE) ? POWER_BUFFER_SIZE : count;

    /* Copy with offset removal: convert 0-4095 to -2048 to +2047 */
    for (i = 0U; i < copyCount; i++)
    {
        destBuffer[i] = (int16_t)((int32_t)adcBuffer[i] - ADC_MIDPOINT);
    }
}
