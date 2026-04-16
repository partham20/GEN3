//#############################################################################
//! \file thd_config.h
//! \brief Configuration constants for the THD Analyzer
//!
//! All tunable parameters in one place. Include this from any module that
//! needs access to system or FFT configuration.
//#############################################################################

#ifndef THD_CONFIG_H
#define THD_CONFIG_H

// ---- System Clock ----
#define THD_SYSCLK_FREQ         150000000U

// ---- Sampling ----
#define THD_SAMPLING_FREQ       6400U
#define THD_EPWM_TIMER_TBPRD    ((THD_SYSCLK_FREQ / THD_SAMPLING_FREQ) - 1)

// ---- FFT ----
#define THD_FFT_STAGES          8
#define THD_FFT_SIZE            (1 << THD_FFT_STAGES)  // 256

// ---- Channels ----
#define THD_NUM_CHANNELS        18

//// ---- Channel Index Mapping ----
//// Output Voltage THD: channels 0, 1, 2 (R, Y, B)
//#define THD_BRANCH_0           0
//#define THD_BRANCH_1           1
//#define THD_BRANCH_2           2
//#define THD_BRANCH_3           3
//#define THD_BRANCH_4           4
//#define THD_BRANCH_5           5
//#define THD_BRANCH_6           6
//#define THD_BRANCH_7           7
//#define THD_BRANCH_8           8
//#define THD_BRANCH_9           9
//#define THD_BRANCH_10          10
//#define THD_BRANCH_11          11
//#define THD_BRANCH_12          12
//#define THD_BRANCH_13          13
//#define THD_BRANCH_14          14
//#define THD_BRANCH_15          15
//#define THD_BRANCH_16          16
//#define THD_BRANCH_17          17
// ---- Analysis Parameters ----
#define THD_FUNDAMENTAL_FREQ    50.0f
#define THD_FUNDAMENTAL_BIN     4       // = THD_FUNDAMENTAL_FREQ / (THD_SAMPLING_FREQ / THD_FFT_SIZE)
#define THD_FUND_BIN_START      3       // Fundamental energy window start
#define THD_FUND_BIN_END        5       // Fundamental energy window end
#define THD_HARMONIC_MIN        2       // First harmonic to include in THD
#define THD_HARMONIC_MAX        15      // Last harmonic to include in THD
#define THD_HARMONIC_WINDOW     1       // +/- bins around each harmonic center
#define THD_FUND_MIN_MAG        1.0f    // Minimum fundamental magnitude to compute THD

// ---- ADC ----
#define THD_ADC_VREF            3.3f
#define THD_ADC_RESOLUTION      4096.0f
#define THD_ADC_SCALE           (THD_ADC_VREF / THD_ADC_RESOLUTION)

// ---- Math ----
#ifndef PI
#define PI                      3.14159265358979f
#endif

#endif // THD_CONFIG_H
