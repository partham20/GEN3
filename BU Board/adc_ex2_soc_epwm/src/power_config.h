/*
 * power_config.h
 *
 * Central Configuration File for Sample Count and Derived Values
 *
 * This file provides a SINGLE SOURCE OF TRUTH for the sample count.
 * All buffer sizes, CAN frame counts, and loop bounds are derived from
 * the values defined here.
 *
 * To change the sample count:
 *   1. Modify CYCLES_TO_CAPTURE (e.g., from 10 to 5)
 *   2. All other values automatically adjust
 *
 * Created: 2026-01-19
 */

#ifndef POWER_CONFIG_H_
#define POWER_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================== */
/*                      PRIMARY CONFIGURATION PARAMETERS                       */
/* ========================================================================== */

/**
 * @brief   Number of ADC samples per AC cycle
 * @note    At 50Hz with 5.12kHz sample rate: 5120/50 = 102.4 samples/cycle
 *          Using 128 samples/cycle for power-of-2 alignment
 */
#define SAMPLES_PER_CYCLE           (128U)

/**
 * @brief   Number of AC cycles to capture per measurement window
 * @note    Change this value to adjust total sample count
 *          10 cycles = 200ms window at 50Hz
 *          5 cycles = 100ms window at 50Hz
 */
#define CYCLES_TO_CAPTURE           (2U)

/**
 * @brief   DC offset for ADC current measurements (CT channels)
 * @note    12-bit ADC with 1.65V reference centered at mid-scale
 *          Ideal value is 2048, but actual offset may vary due to hardware
 *          Adjust this value based on calibration
 */
#define CT_DC_OFFSET                (2010)

/* ========================================================================== */
/*                         DERIVED SAMPLE COUNT                                */
/* ========================================================================== */

/**
 * @brief   Total number of samples per capture window (per phase)
 * @note    = SAMPLES_PER_CYCLE × CYCLES_TO_CAPTURE
 *          Examples:
 *            2 cycles:  128 × 2  = 256 samples
 *            5 cycles:  128 × 5  = 640 samples
 *            10 cycles: 128 × 10 = 1280 samples
 */
#define TOTAL_SAMPLE_COUNT          (SAMPLES_PER_CYCLE * CYCLES_TO_CAPTURE)

/* ========================================================================== */
/*                        DERIVED CAN FRAME PARAMETERS                         */
/* ========================================================================== */

/**
 * @brief   Number of 16-bit voltage values per CAN-FD frame
 * @note    CAN-FD frame: 64 bytes total
 *          Byte 0: Phase ID, Byte 1: Frame number
 *          Bytes 2-63: 31 voltage values (31 × 2 = 62 bytes)
 */
#define CAN_VALUES_PER_FRAME        (31U)

/**
 * @brief   Total number of CAN frames needed to transmit all samples
 * @note    Calculated as ceiling(TOTAL_SAMPLE_COUNT / CAN_VALUES_PER_FRAME)
 *          With 1280 samples: ceil(1280/31) = 42 frames
 *          With 640 samples: ceil(640/31) = 21 frames
 */
#define CAN_TOTAL_FRAMES            ((TOTAL_SAMPLE_COUNT + CAN_VALUES_PER_FRAME - 1U) / CAN_VALUES_PER_FRAME)

/**
 * @brief   Number of values in the last CAN frame
 * @note    Last frame may be partially filled
 *          = TOTAL_SAMPLE_COUNT - (CAN_TOTAL_FRAMES - 1) × CAN_VALUES_PER_FRAME
 *          With 1280 samples: 1280 - (41 × 31) = 1280 - 1271 = 9 values
 */
#define CAN_LAST_FRAME_VALUES       (TOTAL_SAMPLE_COUNT - ((CAN_TOTAL_FRAMES - 1U) * CAN_VALUES_PER_FRAME))

/* ========================================================================== */
/*                           BUFFER SIZE ALIASES                               */
/* ========================================================================== */

/**
 * @brief   Buffer size for ADC results (DMA destination)
 */
#define RESULTS_BUFFER_SIZE         TOTAL_SAMPLE_COUNT

/**
 * @brief   Buffer size for power calculation ping-pong buffers
 */
#define POWER_BUFFER_SIZE           TOTAL_SAMPLE_COUNT

/**
 * @brief   Buffer size for voltage RX (from CAN)
 */
#define VOLTAGE_BUFFER_SIZE         TOTAL_SAMPLE_COUNT

/**
 * @brief   Number of CAN frames for voltage reception
 */
#define VOLTAGE_TOTAL_FRAMES        CAN_TOTAL_FRAMES

/**
 * @brief   Values per frame for voltage reception
 */
#define VOLTAGE_VALUES_PER_FRAME    CAN_VALUES_PER_FRAME

/* ========================================================================== */
/*                           MEMORY CALCULATIONS                               */
/* ========================================================================== */

/**
 * @brief   Size of each sample buffer in bytes
 * @note    Each sample is 16-bit (2 bytes)
 */
#define SAMPLE_BUFFER_BYTES         (TOTAL_SAMPLE_COUNT * 2U)

/**
 * @brief   Total RAM needed for voltage ping-pong buffers (A + B)
 */
#define VOLTAGE_PINGPONG_BYTES      (SAMPLE_BUFFER_BYTES * 2U)

/**
 * @brief   Total RAM needed for current ping-pong buffers (A + B)
 */
#define CURRENT_PINGPONG_BYTES      (SAMPLE_BUFFER_BYTES * 2U)

/* ========================================================================== */
/*                         TIMING CALCULATIONS                                 */
/* ========================================================================== */

/**
 * @brief   Sample rate in Hz (from PWM configuration)
 * @note    SYSCLK=120MHz, PWM Period=23437 → 120MHz/23437 ≈ 5120 Hz
 */
#define SAMPLE_RATE_HZ              (5120U)

/**
 * @brief   Capture window duration in milliseconds
 * @note    = (TOTAL_SAMPLE_COUNT / SAMPLE_RATE_HZ) × 1000
 *          With 1280 samples at 5120Hz: 250ms
 */
#define CAPTURE_WINDOW_MS           ((TOTAL_SAMPLE_COUNT * 1000U) / SAMPLE_RATE_HZ)

#ifdef __cplusplus
}
#endif

#endif /* POWER_CONFIG_H_ */
