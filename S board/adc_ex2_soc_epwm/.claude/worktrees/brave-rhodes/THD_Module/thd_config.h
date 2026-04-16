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
#define THD_FFT_SIZE            (1 << THD_FFT_STAGES)  // 512

// ---- Channels ----
#define THD_NUM_CHANNELS        12

// ---- Channel Index Mapping ----
// Output Voltage THD: channels 0, 1, 2 (R, Y, B)
#define THD_CH_OUT_VOLT_R       0
#define THD_CH_OUT_VOLT_Y       1
#define THD_CH_OUT_VOLT_B       2
// Output Current THD: channels 3, 4, 5 (R, Y, B)
#define THD_CH_OUT_CURR_R       3
#define THD_CH_OUT_CURR_Y       4
#define THD_CH_OUT_CURR_B       5
// Input Voltage THD: channels 6, 7, 8 (R, Y, B)
#define THD_CH_IN_VOLT_R        6
#define THD_CH_IN_VOLT_Y        7
#define THD_CH_IN_VOLT_B        8
// Input Current THD: channels 9, 10, 11 (R, Y, B)
#define THD_CH_IN_CURR_R        9
#define THD_CH_IN_CURR_Y        10
#define THD_CH_IN_CURR_B        11

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
