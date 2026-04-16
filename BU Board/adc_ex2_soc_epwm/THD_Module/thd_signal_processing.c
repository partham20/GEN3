//#############################################################################
//! \file thd_signal_processing.c
//! \brief Portable signal processing functions for THD analysis
//#############################################################################

#include "thd_signal_processing.h"
#include "math.h"

// ---- Window Generation ----
void THD_SP_initWindow(float32_t* windowCoef, int size)
{
    int i;
    for(i = 0; i < size; i++)
    {
        windowCoef[i] = 0.5f * (1.0f - cosf((2.0f * PI * (float32_t)i)
                        / ((float32_t)(size - 1))));
    }
}

// ---- Window Application ----
void THD_SP_applyWindow(float32_t* buffer, const float32_t* windowCoef, int size)
{
    int i;
    for(i = 0; i < size; i++)
    {
        buffer[i] = buffer[i] * windowCoef[i];
    }
}

// ---- DC Offset Removal ----
void THD_SP_removeDCOffset(float32_t* buffer, int size, float32_t* dcBias)
{
    float32_t sum = 0.0f;
    int i;

    for(i = 0; i < size; i++)
    {
        sum += buffer[i];
    }

    float32_t mean = sum / (float32_t)size;

    if(dcBias != (void*)0)
    {
        *dcBias = mean;
    }

    for(i = 0; i < size; i++)
    {
        buffer[i] = buffer[i] - mean;
    }
}

// ---- THD Calculation ----
float32_t THD_SP_calculateTHD(const float32_t* magBuff, const THD_AnalysisConfig* cfg)
{
    float32_t fundEnergy = 0.0f;
    int i;

    // 1. Fundamental energy (sum bins in fundamental window)
    for(i = cfg->fundBinStart; i <= cfg->fundBinEnd; i++)
    {
        fundEnergy += (magBuff[i] * magBuff[i]);
    }

    float32_t fundMag = sqrtf(fundEnergy);

    if(fundMag < cfg->fundMinMag)
    {
        return 0.0f;
    }

    // 2. Sum harmonic energy
    float32_t distortionEnergy = 0.0f;
    int h;

    for(h = cfg->harmonicMin; h <= cfg->harmonicMax; h++)
    {
        int centerBin = cfg->fundamentalBin * h;
        int h_start = centerBin - cfg->harmonicWindow;
        int h_end   = centerBin + cfg->harmonicWindow;

        if(h_end >= cfg->halfFFTSize)
        {
            break;
        }

        for(i = h_start; i <= h_end; i++)
        {
            distortionEnergy += (magBuff[i] * magBuff[i]);
        }
    }

    return sqrtf(distortionEnergy) / fundMag;
}

// ---- Default Config ----
void THD_SP_getDefaultConfig(THD_AnalysisConfig* cfg)
{
    cfg->fundamentalBin = THD_FUNDAMENTAL_BIN;
    cfg->fundBinStart   = THD_FUND_BIN_START;
    cfg->fundBinEnd     = THD_FUND_BIN_END;
    cfg->harmonicMin    = THD_HARMONIC_MIN;
    cfg->harmonicMax    = THD_HARMONIC_MAX;
    cfg->harmonicWindow = THD_HARMONIC_WINDOW;
    cfg->halfFFTSize    = THD_FFT_SIZE / 2;
    cfg->fundMinMag     = THD_FUND_MIN_MAG;
}
