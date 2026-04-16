/*
 * power_calc.h
 *
 * Power Calculation Module with Ping-Pong Buffers
 * Calculates Real Power from instantaneous voltage and current samples
 *
 * Real Power = (1/N) * SUM(V[i] * I[i]) for i = 0 to N-1
 *
 * Features:
 *   - Ping-pong buffers for voltage (from CAN) and current (from ADC)
 *   - 64-bit accumulator to prevent overflow
 *   - Configurable scaling factors for real-world units
 */

#ifndef POWER_CALC_H_
#define POWER_CALC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "power_config.h"  /* Central configuration for sample count */

/* ========================================================================== */
/*                                  Macros                                    */
/* ========================================================================== */

/* POWER_BUFFER_SIZE is defined in power_config.h as TOTAL_SAMPLE_COUNT */

/* Number of ping-pong buffers */
#define NUM_PINGPONG_BUFFERS        (2U)

/* ADC midpoint for offset removal (12-bit ADC: 4096/2 = 2048) */
#define ADC_MIDPOINT                (2010)
#define FILTER_ALPHA 0.01f
/* Scaling factors for real-world units (adjust based on your sensors)
 *
 * Example calculation:
 *   If voltage sensor: 230V RMS maps to 1.65V DC offset with 1.5V peak-to-peak
 *   ADC: 3.3V / 4096 = 0.000806V per LSB
 *   V_SCALE = (230 * sqrt(2)) / (1.5 / 0.000806) = 0.175 (example)
 *
 *   If current sensor (e.g., ACS712-30A): 30A maps to 1.65V offset with 1.5V p-p
 *   I_SCALE = (30 * sqrt(2)) / (1.5 / 0.000806) = 0.0228 (example)
 *
 * For raw ADC unit calculations, set both to 1.0
 */
#define VOLTAGE_SCALE_FACTOR        (1.0f)
#define CURRENT_SCALE_FACTOR        (0.00001388175f)

/* Power scale factor = V_SCALE * I_SCALE */
#define POWER_SCALE_FACTOR          (VOLTAGE_SCALE_FACTOR * CURRENT_SCALE_FACTOR)

/* ========================================================================== */
/*                             Type Definitions                               */
/* ========================================================================== */

/* Ping-pong buffer index */
typedef enum {
    BUFFER_A = 0,
    BUFFER_B = 1
} PingPongIndex;

/* Power calculation status */
typedef struct {
    bool     isReady;           /* Power calculation complete flag */
    bool     isValid;           /* Data validity flag */
    uint32_t sampleCount;       /* Number of samples used in calculation */
    uint32_t cycleCount;        /* Number of calculation cycles completed */
} PowerCalc_Status;

/* Power calculation results */
typedef struct {
    int32_t  realPower_raw;     /* Real power in raw ADC units (V*I average) */
    float    realPower_watts;   /* Real power in Watts (if scaling applied) */
    int64_t  accumulator;       /* Raw accumulator value (for debugging) */
} PowerCalc_Results;

/* ========================================================================== */
/*                          Global Variables                                  */
/* ========================================================================== */

/* Ping-pong buffers for voltage (from CAN) */
extern volatile int16_t g_voltageBuffer_A[POWER_BUFFER_SIZE];
extern volatile int16_t g_voltageBuffer_B[POWER_BUFFER_SIZE];

/* Ping-pong buffers for current (from ADC/DMA) */
extern volatile int16_t g_currentBuffer_A[POWER_BUFFER_SIZE];
extern volatile int16_t g_currentBuffer_B[POWER_BUFFER_SIZE];

/* Active buffer indices */
extern volatile PingPongIndex g_activeVoltageBuffer;  /* Buffer being written by CAN RX */
extern volatile PingPongIndex g_activeCurrentBuffer;  /* Buffer being written by DMA */

/* Power calculation status and results */
extern volatile PowerCalc_Status g_powerStatus;
extern volatile PowerCalc_Results g_powerResults;

/* Flag indicating new power data is available */
extern volatile bool g_newPowerDataAvailable;
extern volatile int16_t *voltageBuffer;
extern volatile int16_t *currentBuffer;

/* ========================================================================== */
/*                          Function Declarations                             */
/* ========================================================================== */

/**
 * @brief   Initialize the power calculation module
 *          Clears all buffers and resets status
 */
void PowerCalc_init(void);

/**
 * @brief   Reset the power calculation module
 *          Clears buffers and status without full re-initialization
 */
void PowerCalc_reset(void);

/**
 * @brief   Get pointer to the active voltage write buffer
 * @return  Pointer to the buffer currently being written by CAN RX
 */
volatile int16_t* PowerCalc_getActiveVoltageBuffer(void);

/**
 * @brief   Get pointer to the inactive voltage buffer (for reading/calculation)
 * @return  Pointer to the buffer with complete data from previous cycle
 */
volatile int16_t* PowerCalc_getInactiveVoltageBuffer(void);

/**
 * @brief   Get pointer to the active current write buffer
 * @return  Pointer to the buffer currently being written by DMA
 */
volatile int16_t* PowerCalc_getActiveCurrentBuffer(void);

/**
 * @brief   Get pointer to the inactive current buffer (for reading/calculation)
 * @return  Pointer to the buffer with complete data from previous cycle
 */
volatile int16_t* PowerCalc_getInactiveCurrentBuffer(void);

/**
 * @brief   Swap the active voltage buffer (call after CAN RX complete)
 */
void PowerCalc_swapVoltageBuffer(void);

/**
 * @brief   Swap the active current buffer (call after DMA complete)
 */
void PowerCalc_swapCurrentBuffer(void);

/**
 * @brief   Calculate real power from the inactive (complete) buffers
 *          P_real = (1/N) * SUM(V[i] * I[i])
 *
 * @note    This function should be called after both voltage and current
 *          buffers have been filled and swapped.
 *          Uses 64-bit accumulator to prevent overflow.
 *          Runs from RAM for speed.
 */
void PowerCalc_calculateRealPower(void);

/**
 * @brief   Get the calculated real power in raw ADC units
 * @return  Real power as (V*I) average in raw ADC units
 */
int32_t PowerCalc_getRealPower_raw(void);

/**
 * @brief   Get the calculated real power in Watts
 * @return  Real power in Watts (with scaling factors applied)
 */
float PowerCalc_getRealPower_watts(void);

/**
 * @brief   Check if new power calculation data is available
 * @return  true if new data available, false otherwise
 */
bool PowerCalc_isDataReady(void);

/**
 * @brief   Clear the data ready flag (call after reading power data)
 */
void PowerCalc_clearDataReady(void);

/**
 * @brief   Copy ADC buffer to current ping-pong buffer with offset removal
 *          Converts uint16_t ADC values to int16_t with -2048 offset
 *
 * @param   adcBuffer   Pointer to source ADC buffer (uint16_t)
 * @param   count       Number of samples to copy
 */
void PowerCalc_copyCurrentFromADC(const uint16_t *adcBuffer, uint16_t count);

#ifdef __cplusplus
}
#endif

#endif /* POWER_CALC_H_ */


