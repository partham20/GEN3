//#############################################################################
//! \file thd_analyzer.h
//! \brief High-level THD Analyzer API
//!
//! This is the main integration point. External code can:
//!   1. Call THD_init() once at startup
//!   2. Feed raw sample data via THD_loadChannelData()
//!   3. Call THD_processChannel() to run FFT + THD on one channel
//!   4. Read results via THD_getResult()
//!
//! Or use THD_processAllChannels() to process all channels at once.
//!
//! Example integration:
//!   #include "thd_analyzer.h"
//!
//!   THD_Analyzer analyzer;
//!   THD_init(&analyzer);
//!
//!   // Feed data from your own ADC/source
//!   THD_loadChannelData(&analyzer, 0, myAdcBuffer, 512);
//!   THD_processChannel(&analyzer, 0);
//!
//!   float thd_pct = THD_getResult(&analyzer, 0)->thdPercent;
//!   float fundamental = THD_getResult(&analyzer, 0)->fundamentalMag;
//#############################################################################

#ifndef THD_ANALYZER_H
#define THD_ANALYZER_H

#include "thd_config.h"
#include "thd_signal_processing.h"
#include "c2000ware_libraries.h"

// ---- Per-Channel Result ----
typedef struct {
    float32_t thdPercent;       // THD as percentage (0-100+)
    float32_t fundamentalMag;   // Magnitude at fundamental frequency bin
    float32_t dcBias;           // Detected DC offset
} THD_ChannelResult;

// ---- Analyzer Instance ----
// Contains all state needed for FFT-based THD analysis.
// Multiple instances can coexist if needed (e.g., different FFT sizes).
typedef struct {
    // FFT handle (from C2000Ware DSP library)
    RFFT_F32_STRUCT         rfftObj;
    RFFT_F32_STRUCT_Handle  rfftHandle;

    // Buffers - these point to the aligned memory declared by the user
    float32_t*  inBuf;          // FFT input buffer (must be aligned)
    float32_t*  outBuf;         // FFT output buffer
    float32_t*  magBuf;         // Magnitude spectrum buffer
    float32_t*  phaseBuf;       // Phase spectrum buffer
    float32_t*  twiddleBuf;     // Twiddle factor buffer
    float32_t*  windowCoef;     // Window coefficients

    // Raw channel data storage
    float32_t*  channelData;    // Pointer to [NUM_CHANNELS][FFT_SIZE] array
    int         numChannels;
    int         fftSize;

    // Analysis config
    THD_AnalysisConfig  analysisCfg;

    // Results
    THD_ChannelResult   results[THD_NUM_CHANNELS];

    // State
    int initialized;
} THD_Analyzer;

// ---- Initialization ----

//! Initialize the analyzer with external buffers.
//! The caller must provide pre-allocated, properly aligned buffers.
//! \param[out] analyzer      Analyzer instance to initialize
//! \param[in]  inBuf         FFT input buffer (aligned to 2*fftSize bytes)
//! \param[in]  outBuf        FFT output buffer (fftSize elements)
//! \param[in]  magBuf        Magnitude buffer (fftSize/2+1 elements)
//! \param[in]  phaseBuf      Phase buffer (fftSize/2 elements)
//! \param[in]  twiddleBuf    Twiddle coefficients (fftSize elements)
//! \param[in]  windowCoef    Window coefficients (fftSize elements)
//! \param[in]  channelData   Raw data storage [numChannels * fftSize]
//! \param[in]  numChannels   Number of input channels
//! \param[in]  fftSize       FFT size (must be power of 2)
//! \param[in]  fftStages     log2(fftSize)
void THD_init(THD_Analyzer* analyzer,
              float32_t* inBuf,
              float32_t* outBuf,
              float32_t* magBuf,
              float32_t* phaseBuf,
              float32_t* twiddleBuf,
              float32_t* windowCoef,
              float32_t* channelData,
              int numChannels,
              int fftSize,
              int fftStages);

// ---- Data Loading ----

//! Copy raw samples into a channel's data buffer.
//! \param[in]  analyzer   Analyzer instance
//! \param[in]  channel    Channel index (0 to numChannels-1)
//! \param[in]  samples    Source sample buffer
//! \param[in]  count      Number of samples (should equal fftSize)
void THD_loadChannelData(THD_Analyzer* analyzer, int channel,
                         const float32_t* samples, int count);

// ---- Processing ----

//! Process a single channel: DC removal -> windowing -> FFT -> magnitude -> THD
//! Results are stored internally; retrieve with THD_getResult().
//! \param[in]  analyzer   Analyzer instance
//! \param[in]  channel    Channel index to process
void THD_processChannel(THD_Analyzer* analyzer, int channel);

//! Process all channels sequentially.
//! \param[in]  analyzer   Analyzer instance
void THD_processAllChannels(THD_Analyzer* analyzer);

// ---- Results ----

//! Get the result for a specific channel.
//! \param[in]  analyzer   Analyzer instance
//! \param[in]  channel    Channel index
//! \return     Pointer to the channel's result struct (valid until next process call)
const THD_ChannelResult* THD_getResult(const THD_Analyzer* analyzer, int channel);

//! Override the analysis configuration (harmonic range, bin positions, etc.)
//! \param[in]  analyzer   Analyzer instance
//! \param[in]  cfg        New configuration to apply
void THD_setAnalysisConfig(THD_Analyzer* analyzer, const THD_AnalysisConfig* cfg);

#endif // THD_ANALYZER_H
