/*
 * Copyright (c) 2023, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

//*****************************************************************************
// the includes
//*****************************************************************************
#include <stdint.h>
#include "inc/hw_types.h"
#include "metrology_defines.h"
#include "metrology_structs.h"
#include "metrology_background.h"

/*!
 * @brief log phase parameters
 * @param[in] workingData The metrology data
 * @param[in] ph          Phase number
 */
void logPhaseParameters(metrologyData *workingData, int ph)
{
    phaseMetrology *phase = &workingData->phases[ph];

    /* Check current overrange */
    if(phase->params.current.currentEndStops <= 0)
    {
        phase->status |= PHASE_STATUS_I_OVERRANGE;
    }
    else
    {
        phase->status &= ~PHASE_STATUS_I_OVERRANGE;
    }

    /* Reload the limited number of hits value */
    phase->params.current.currentEndStops = ENDSTOP_HITS_FOR_OVERLOAD;

    /* Reset the dpset data for next sampling */
    uint8_t dp = phase->params.dpSet;
    phase->params.current.dotProd[dp].currentSq = 0.0f;
    phase->params.current.dotProd[dp].sampleCount = 0;
    phase->params.dpSet ^= 1;

    phase->status |= PHASE_STATUS_NEW_LOG;
}

/*!
 * @brief Per sample processing for RMS Current
 * @param[in] workingData The metrology data
 */
void Metrology_perSampleProcessing(metrologyData *workingData)
{
    float currentSample;
    float currentData;
    currentDPSet *currentDP;
    uint32_t phLog = 0;

    PHASES ph;
    for(ph = BRANCH_CT0; ph < MAX_PHASES; ph++)
    {
        phaseMetrology *phase = &workingData->phases[ph];

        uint8_t dp = (phase->params.dpSet ^ 0x01) & 0x01;
        currentDP = &phase->params.current.dotProd[dp];

        /* Get current sample and apply DC filter */
        currentData = metrology_dcFilter(&(phase->params.current.I_dc_estimate), workingData->rawCurrentData[ph]);

        currentSample = phase->params.current.IHistory[0];

        /* Check for overrange */
        if((currentSample >= I_ADC_MAX || currentSample <= I_ADC_MIN) && phase->params.current.currentEndStops)
        {
            phase->params.current.currentEndStops--;
        }

        /* Update history buffer */
        phase->params.current.IHistory[0] = phase->params.current.IHistory[1];
        phase->params.current.IHistory[1] = currentData;

        /* Accumulate current squared for RMS calculation */
        #ifdef IRMS_SUPPORT
            currentDP->currentSq += (currentSample * currentSample);
            ++currentDP->sampleCount;
        #endif

        /* Check if enough samples collected */
        if(currentDP->sampleCount >= SAMPLE_RATE)
        {
            phLog |= 1UL;
        }

        phLog <<= 1;
    }

    /* Log phase parameters when sampling is done */
    if(phLog)
    {
        PHASES ph;
        for(ph = BRANCH_CT0; ph < MAX_PHASES; ph++)
        {
            if(phLog & (1UL << (MAX_PHASES - ph)))
            {
                logPhaseParameters(workingData, ph);
            }
        }
    }
}
