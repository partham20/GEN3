//#############################################################################
//! \file thd_signal_processing.h
//! \brief Portable signal processing functions for THD analysis
//!
//! Platform-independent DSP utilities: windowing, DC removal, and THD
//! calculation. These functions operate on float32_t buffers and can be
//! reused in any C project.
//!
//! Usage from external code:
//!   #include "thd_signal_processing.h"
//!   THD_SP_initWindow(windowBuf, 512);
//!   THD_SP_removeDCOffset(buffer, 512, &dcBias);
//!   THD_SP_applyWindow(buffer, windowBuf, 512);
//!   float thd = THD_SP_calculateTHD(magBuf, &cfg);
//#############################################################################

#ifndef THD_SIGNAL_PROCESSING_H
#define THD_SIGNAL_PROCESSING_H

#include "thd_config.h"
#include "driverlib.h"

// ---- THD Calculation Configuration ----
// Pass this struct to THD_SP_calculateTHD to control analysis parameters.
// Allows the caller to customize without recompiling.
typedef struct {
    int     fundamentalBin;     // FFT bin of fundamental frequency
    int     fundBinStart;       // Start of fundamental energy window
    int     fundBinEnd;         // End of fundamental energy window
    int     harmonicMin;        // First harmonic number (e.g., 2)
    int     harmonicMax;        // Last harmonic number (e.g., 15)
    int     harmonicWindow;     // +/- bins around each harmonic center
    int     halfFFTSize;        // RFFT_SIZE / 2
    float32_t fundMinMag;       // Minimum fundamental magnitude threshold
} THD_AnalysisConfig;

// ---- API Functions ----

//! Initialize a Hann window coefficient array
//! \param[out] windowCoef  Output buffer (must be at least 'size' elements)
//! \param[in]  size        Number of FFT points
void THD_SP_initWindow(float32_t* windowCoef, int size);

//! Apply a window function to a signal buffer (in-place)
//! \param[in,out] buffer     Signal buffer to window
//! \param[in]     windowCoef Pre-computed window coefficients
//! \param[in]     size       Number of samples
void THD_SP_applyWindow(float32_t* buffer, const float32_t* windowCoef, int size);

//! Remove DC offset from a signal buffer (in-place)
//! \param[in,out] buffer   Signal buffer
//! \param[in]     size     Number of samples
//! \param[out]    dcBias   If non-NULL, stores the detected DC bias value
void THD_SP_removeDCOffset(float32_t* buffer, int size, float32_t* dcBias);

//! Calculate THD from a magnitude spectrum
//! \param[in]  magBuff  Magnitude spectrum from RFFT
//! \param[in]  cfg      Analysis configuration (bin positions, harmonic range)
//! \return     THD as a ratio (0.0 to ~1.0+). Multiply by 100 for percentage.
float32_t THD_SP_calculateTHD(const float32_t* magBuff, const THD_AnalysisConfig* cfg);

//! Fill a THD_AnalysisConfig with default values from thd_config.h
//! \param[out] cfg  Configuration struct to populate
void THD_SP_getDefaultConfig(THD_AnalysisConfig* cfg);

//! Estimate fundamental frequency from magnitude spectrum using parabolic interpolation
//! Searches bins 1-3 (25-75 Hz for 256-pt FFT at 6400 Hz) independent of THD config.
//! \param[in]  magBuff       Magnitude spectrum from RFFT
//! \param[in]  samplingFreq  ADC sampling frequency in Hz
//! \param[in]  fftSize       FFT size (number of points)
//! \return     Estimated frequency in Hz (0.0 if signal too weak)
float32_t THD_SP_estimateFrequency(const float32_t* magBuff,
                                   float32_t samplingFreq, int fftSize);

#endif // THD_SIGNAL_PROCESSING_H
