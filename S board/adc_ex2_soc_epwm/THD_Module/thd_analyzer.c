//#############################################################################
//! \file thd_analyzer.c
//! \brief High-level THD Analyzer implementation
//#############################################################################

#include "thd_analyzer.h"
#include "string.h"

// ---- Initialization ----
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
              int fftStages)
{
    // Store buffer pointers
    analyzer->inBuf       = inBuf;
    analyzer->outBuf      = outBuf;
    analyzer->magBuf      = magBuf;
    analyzer->phaseBuf    = phaseBuf;
    analyzer->twiddleBuf  = twiddleBuf;
    analyzer->windowCoef  = windowCoef;
    analyzer->channelData = channelData;
    analyzer->numChannels = numChannels;
    analyzer->fftSize     = fftSize;

    // Configure RFFT handle
    analyzer->rfftHandle = &analyzer->rfftObj;
    analyzer->rfftHandle->InBuf     = inBuf;
    analyzer->rfftHandle->OutBuf    = outBuf;
    analyzer->rfftHandle->MagBuf    = magBuf;
    analyzer->rfftHandle->PhaseBuf  = phaseBuf;
    analyzer->rfftHandle->CosSinBuf = twiddleBuf;
    analyzer->rfftHandle->FFTSize   = (uint16_t)fftSize;
    analyzer->rfftHandle->FFTStages = (uint16_t)fftStages;

    // Generate twiddle factors
    RFFT_f32_sincostable(analyzer->rfftHandle);

    // Generate window coefficients
    THD_SP_initWindow(windowCoef, fftSize);

    // Set default analysis config
    THD_SP_getDefaultConfig(&analyzer->analysisCfg);

    // Clear buffers
    memset(inBuf, 0, fftSize * sizeof(float32_t));
    memset(analyzer->results, 0, sizeof(analyzer->results));

    analyzer->initialized = 1;
}

// ---- Data Loading ----
void THD_loadChannelData(THD_Analyzer* analyzer, int channel,
                         const float32_t* samples, int count)
{
    if(channel < 0 || channel >= analyzer->numChannels) return;
    if(count > analyzer->fftSize) count = analyzer->fftSize;

    memcpy(&analyzer->channelData[channel * analyzer->fftSize],
           samples, count * sizeof(float32_t));
}

// ---- Single Channel Processing ----
void THD_processChannel(THD_Analyzer* analyzer, int channel)
{
    if(channel < 0 || channel >= analyzer->numChannels) return;

    // 1. Copy channel data into FFT input buffer
    memcpy(analyzer->inBuf,
           &analyzer->channelData[channel * analyzer->fftSize],
           analyzer->fftSize * sizeof(float32_t));

    // 2. Remove DC offset
    THD_SP_removeDCOffset(analyzer->inBuf, analyzer->fftSize,
                          &analyzer->results[channel].dcBias);

    // 3. Apply window
    THD_SP_applyWindow(analyzer->inBuf, analyzer->windowCoef, analyzer->fftSize);

    // 4. Run FFT
    RFFT_f32(analyzer->rfftHandle);

    // 5. Compute magnitude spectrum
    #ifdef __TMS320C28XX_TMU__
        RFFT_f32_mag_TMU0(analyzer->rfftHandle);
    #else
        RFFT_f32_mag(analyzer->rfftHandle);
    #endif

    // 6. Calculate THD
    analyzer->results[channel].thdPercent =
        THD_SP_calculateTHD(analyzer->magBuf, &analyzer->analysisCfg) * 100.0f;

    // 7. Store fundamental magnitude
    analyzer->results[channel].fundamentalMag =
        analyzer->magBuf[analyzer->analysisCfg.fundamentalBin];

    // 8. Estimate fundamental frequency via parabolic interpolation
    analyzer->results[channel].frequencyHz =
        THD_SP_estimateFrequency(analyzer->magBuf,
                                 (float32_t)THD_SAMPLING_FREQ, analyzer->fftSize);
}

// ---- Process All Channels ----
void THD_processAllChannels(THD_Analyzer* analyzer)
{
    int ch;
    for(ch = 0; ch < analyzer->numChannels; ch++)
    {
        THD_processChannel(analyzer, ch);
    }
}

// ---- Get Result ----
const THD_ChannelResult* THD_getResult(const THD_Analyzer* analyzer, int channel)
{
    if(channel < 0 || channel >= analyzer->numChannels) return (void*)0;
    return &analyzer->results[channel];
}

// ---- Set Analysis Config ----
void THD_setAnalysisConfig(THD_Analyzer* analyzer, const THD_AnalysisConfig* cfg)
{
    analyzer->analysisCfg = *cfg;
}
