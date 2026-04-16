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

volatile uint32_t g_dcFilterStartupCount = 0;

/*!
 * @brief log phase parameters
 * @param[in] workingData The metrology data
 * @param[in] ph          Phase number
 */
void logPhaseParameters(metrologyData *workingData, int ph)
{
    phaseMetrology *phase = &workingData->phases[ph];

    /*
     * If the voltage hits the max ADC value more than limited number of hits
     * it is considered as over range.
     */
    if (phase->params.voltageEndStops <= 0)
    {
        phase->status |= PHASE_STATUS_V_OVERRANGE;
    }
    else
    {
        phase->status &= ~PHASE_STATUS_V_OVERRANGE;
    }

    /* Reload the limited number of hits value */
    phase->params.voltageEndStops = ENDSTOP_HITS_FOR_OVERLOAD;

    /*
     * If the current hits the max ADC value more than limited number of hits
     * it is considered as over range.
     */
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

    if(++ph >= MAX_PHASES)
    {
        ph = 0;
    }

    phase->readings.phasetoPhaseAngle = (((float)workingData->phases[ph].params.purePhase*(0.0000000004656470768866917f)) - ((float)phase->params.purePhase*(0.0000000004656470768866917f)));

    /* Reset the dpset data for next sampling */
    uint8_t dp = phase->params.dpSet;

    phase->params.dotProd[dp].voltageSq = 0.0f;
    phase->params.dotProd[dp].sampleCount = 0;
    phase->params.current.dotProd[dp].currentSq = 0.0f;
    phase->params.current.dotProd[dp].activePower = 0.0f;
    phase->params.current.dotProd[dp].reactivePower = 0.0f;
    phase->params.current.dotProd[dp].sampleCount = 0;

    phase->params.dotProd[dp].fVoltageSq = 0.0f;
    phase->params.dotProd[dp].fVoltageQuardSq = 0.0f;
    phase->params.current.dotProd[dp].FActivePower = 0.0f;
    phase->params.current.dotProd[dp].FReactivePower = 0.0f;
    phase->params.dpSet ^= 1;

    phase->status |= PHASE_STATUS_NEW_LOG;
}

/*!
 * @brief log neutral parameters
 * @param[in] workingData The metrology data
 */
void logNeutralParameters(metrologyData *workingData)
{
    neutralMetrology * neutral = &workingData->neutral;

    /*
     * If the current hits the max ADC value more than limited number of hits
     * it is considered as over range.
     */
    if (neutral->params.currentEndStops <= 0)
    {
        neutral->status |= PHASE_STATUS_I_OVERRANGE;
    }
    else
    {
        neutral->status &= ~PHASE_STATUS_I_OVERRANGE;
    }

    /*
     * Reload the limited number of hits value
     */
    neutral->params.currentEndStops = ENDSTOP_HITS_FOR_OVERLOAD;
    neutral->params.I_dc_estimate_logged = neutral->params.I_dc_estimate;

    /*
     * Reset the dpset data for next sampling
     */
    uint8_t dp = neutral->params.dpSet;
    neutral->params.dotProd[dp].currentSq = 0.0f;
    neutral->params.dotProd[dp].sampleCount = 0;
    neutral->params.dpSet ^= 1;

    neutral->status |= PHASE_STATUS_NEW_LOG;
}

/*!
 * @brief Sine lookup using direct sinf() call for maximum accuracy
 * @param[in] phase 32-bit phase value
 * @return 24-bit signed sine value
 */
float Metrology_directSin(uint32_t phase)
{
    // Convert 32-bit phase to radians (0 to 2Ï€)
    float angle = (float)phase * (2.0f * PI *(0.0000000002328306436538696f));

    // Calculate sine using standard library (maximum accuracy)
    float result = sinf(angle);

    return result;
}

/*!
 * @brief Start active energy pulse
 * @param[in] workingData The metrology Data
 */
void Metrology_startActiveEnergyPulse(metrologyData *workingData)
{
    workingData->activePulse = FALSE;
}

/*!
 * @brief End active energy pulse
 * @param[in] workingData The metrology Data
 */
void Metrology_endActiveEnergyPulse(metrologyData *workingData)
{
    workingData->activePulse = TRUE;
}

/*!
 * @brief Start reactive energy pulse
 * @param[in] workingData The metrology Data
 */
void Metrology_startReactiveEnergyPulse(metrologyData *workingData)
{
    workingData->reactivePulse = FALSE;
}

/*!
 * @brief End reactive energy pulse
 * @param[in] workingData The metrology Data
 */
void Metrology_endReactiveEnergyPulse(metrologyData *workingData)
{
    workingData->reactivePulse = TRUE;
}

void Metrology_perSampleProcessing(metrologyData *workingData)
{
    OPERATING_MODES operatingMode = workingData->operatingMode;

    float voltageSample;
    float voltageCorrected;
    float voltageQuadCorrected;
    float voltagePure;
    float voltageQuardPure;
    float voltageSq;
    float cross_corr;
    float currentSample;
    float currentCorrected;
    float neutralCurrent;

    voltageDPSet *phaseDP;
    cyclePhaseDPSet *cyclePhaseDP;
    currentDPSet *currentDP;
    neutralDPSet *neutralDP;

    /* Increment startup counter for two-speed DC filter */
    if(g_dcFilterStartupCount < SAMPLE_RATE)
    {
        g_dcFilterStartupCount++;
    }

    /* Flag used to store which phase sampling is completed */
    int8_t phLog;

    phLog = 0;

    PHASES ph;
    for(ph = PHASE_ONE; ph < MAX_PHASES; ph++)
    {
        phaseMetrology *phase = &workingData->phases[ph];

        uint8_t dp = (phase->params.dpSet ^ 0x01) & 0x01;
        phaseDP = &phase->params.dotProd[dp];
        uint8_t cycleDp = (phase->params.cycleDpSet ^ 0x01) & 0x01;
        cyclePhaseDP = &phase->params.cycleDotProd[cycleDp];

        voltageSample = workingData->rawVoltageData[ph];
        if((voltageSample >= V_ADC_MAX || voltageSample <= V_ADC_MIN) && phase->params.voltageEndStops)
        {
            phase->params.voltageEndStops--;
        }
        voltageSample = metrology_dcFilter(&(phase->params.V_dc_estimate), voltageSample);

    #ifdef ROGOWSKI_SUPPORT
        /*
         * With rogowski coils, current samples will pass through 2 filters, to compensate the
         * delay between voltage and current samples, voltage samples are also passed through
         * 2 filters.
         */
        voltageSample = metrology_dcFilter(&(phase->params.V_dc_estimate1), voltageSample);
    #endif

    #ifdef VRMS_SUPPORT
        voltageSq = voltageSample * voltageSample;
        phaseDP->voltageSq += voltageSq;
        ++phaseDP->sampleCount;
        #ifdef SAG_SWELL_SUPPORT
            cyclePhaseDP->cycleVoltageSq += voltageSq;
            ++cyclePhaseDP->cycleSampleCount;
        #endif
    #endif

        /* We need to save the history of the voltage signal if we are performing phase correction, and/or
           measuring the quadrature shifted power (to obtain an accurate measure of one form of the reactive power).
        */
        phase->params.vHistory[phase->params.vHistoryIndex] = voltageSample;
        #if defined( HARMONICS_SUPPORT )
          phase->params.vInputBuffer[phase->params.vInputBufferIndex] = voltageSample;
          phase->params.totalSampleCount++;
        #endif

        if(operatingMode == OPERATING_MODE_NORMAL)
        {
            const int prev_index = (phase->params.vHistoryIndex -
                               phase->params.current.phaseCorrection.step - 1) & V_HISTORY_MASK;
            const int next_index = (phase->params.vHistoryIndex -
                               phase->params.current.phaseCorrection.step) & V_HISTORY_MASK;
            const float firBeta = phase->params.current.phaseCorrection.firBeta / 32768.0f;
            voltageCorrected = (phase->params.vHistory[prev_index] * firBeta) +
                              phase->params.vHistory[next_index];

        #ifdef FUNDAMENTAL_VRMS_SUPPORT
            /*
             * Phase-Locked Loop (PLL) implementation for frequency tracking
             *
             * This block implements a software PLL that locks to the grid frequency:
             * 1. A direct digital synthesizer generates reference sine/cosine signals
             * 2. These are correlated with the input voltage to detect phase error
             * 3. The phase error drives a proportional loop filter
             * 4. The filter output adjusts the synthesizer frequency
             * 5. When locked, purePhase tracks the exact grid phase angle
             */

            /* Generate in-phase and quadrature reference signals */
            voltagePure = Metrology_directSin(phase->params.purePhase);    //purePhase: 32-bit phase accumulator (0 to 2π mapped to 0 to 2³²)
            phaseDP->fVoltageSq += (voltageCorrected * voltagePure);
            voltageQuardPure = Metrology_directSin(phase->params.purePhase + 0x40000000);//voltageQuardPure: Quadrature (Q) reference signal = sin(θ + 90°)//0x40000000: Adds 90° phase shift (¼ of 2³² = 1,073,741,824)
            phaseDP->fVoltageQuardSq += (voltageCorrected * voltageQuardPure);

            /* Calculate phase error using cross-correlation (multiplication detector) */
            cross_corr = (voltageCorrected * 8388607.0f) * (voltageQuardPure * 8388607.0f);
            cross_corr *= 0.0000152587890625f;  /* Scale by 2^-16 to normalize */

            /* Loop filter with coefficient Î± = 2^-16 â‰ˆ 0.0000152587890625f */
            const float loop_filter_alpha = 0.0000152587890625f;
            phase->params.crossSum += ((cross_corr - phase->params.crossSum) * loop_filter_alpha);

            /* Update oscillator phase and frequency */
            phase->params.purePhase += (phase->params.crossSum * 0.000244141f); /* VCO gain: 1/4096 */
            phase->params.purePhase += (uint32_t)(phase->params.purePhaseRate); /* Base increment */

        #endif
        }

        currentDP = &phase->params.current.dotProd[dp];

        float currentData;
        float inputCurrent;
        float integratorOutput;

        #ifdef ROGOWSKI_SUPPORT
            inputCurrent = metrology_dcFilter(&(phase->params.current.I_dc_estimate), workingData->rawCurrentData[ph]);
            float intnewcurrent = (inputCurrent + workingData->lastRawCurrentData[ph]) * 0.5f;
            workingData->lastRawCurrentData[ph] = inputCurrent;

            workingData->currentIntegrationData[ph] += intnewcurrent / phase->params.voltagePeriod.cyclePeriod;
            currentData = metrology_dcFilter(&(phase->params.current.I_dc_estimate_integral), workingData->currentIntegrationData[ph]);
        #else
            currentData = metrology_dcFilter(&(phase->params.current.I_dc_estimate), workingData->rawCurrentData[ph]);
        #endif

        currentSample = phase->params.current.IHistory[0];

        if((currentSample >= I_ADC_MAX || currentSample <= I_ADC_MIN) && phase->params.current.currentEndStops)
        {
            phase->params.current.currentEndStops--;
        }
        phase->params.current.IHistory[0] = phase->params.current.IHistory[1];
        phase->params.current.IHistory[1] = currentData;

        #if defined( HARMONICS_SUPPORT )
              phase->params.current.iInputBuffer[ phase->params.current.iInputBufferIndex] = currentData;
              phase->params.current.iInputBufferIndex = (phase->params.current.iInputBufferIndex + 1) & (FFT_SIZE-1);
              phase->params.current.totalSampleCount++;
        #endif

        #ifdef IRMS_SUPPORT
            currentDP->currentSq += (currentSample * currentSample);
            ++currentDP->sampleCount;
        #endif

        if (operatingMode == OPERATING_MODE_NORMAL)
        {
        #ifdef ACTIVE_POWER_SUPPORT
            // Convert fixed-point FIR filter to floating-point
            const int prev_index2 = (phase->params.vHistoryIndex -
                                phase->params.current.phaseCorrection.step - 1) & V_HISTORY_MASK;
            const int next_index2 = (phase->params.vHistoryIndex -
                                phase->params.current.phaseCorrection.step) & V_HISTORY_MASK;

            // Convert Q15 coefficient to float (divide by 2^15)
            const float firBeta = phase->params.current.phaseCorrection.firBeta * (1.0f/32768.0f);

            // Perform the equivalent of _IQ23mpyIQX() in floating-point
            voltageCorrected = (phase->params.vHistory[prev_index2] * firBeta)
                            + phase->params.vHistory[next_index2];

            currentDP->activePower += (currentSample * voltageCorrected);
            #ifdef FUNDAMENTAL_ACTIVE_POWER_SUPPORT
                currentDP->FActivePower += (currentSample * voltagePure);
            #endif
        #endif
        #ifdef REACTIVE_POWER_SUPPORT
            /*
             * Calculate quadrature voltage for reactive power calculation
             * - Uses a circular buffer of voltage history with V_HISTORY_MASK for indexing
             * - Applies a linear interpolation between adjacent samples with firBeta coefficient
             * - The quadrature signal is effectively a 90-degree phase shifted version of voltage
             */
            const int quad_prev_index = (phase->params.vHistoryIndex -
                    phase->params.current.quadratureCorrection.step - 1) & V_HISTORY_MASK;
            const int quad_next_index = (phase->params.vHistoryIndex -
                    phase->params.current.quadratureCorrection.step) & V_HISTORY_MASK;

            voltageQuadCorrected = (phase->params.vHistory[quad_prev_index] *
                                    phase->params.current.quadratureCorrection.firBeta) +
                                    phase->params.vHistory[quad_next_index];

            currentDP->reactivePower += (currentSample * voltageQuadCorrected);

            #ifdef FUNDAMENTAL_REACTIVE_POWER_SUPPORT
                currentDP->FReactivePower += (currentSample * -voltageQuardPure);
            #endif
        #endif
        }
    phase->params.vHistoryIndex = (phase->params.vHistoryIndex + 1) & V_HISTORY_MASK;
    #if defined( HARMONICS_SUPPORT )
          phase->params.vInputBufferIndex = (phase->params.vInputBufferIndex + 1) & (FFT_SIZE-1);
    #endif

/*
* Frequency measurement:
* time between 2 voltage samples is divided into 256(2 power 8) parts
* first will detect the change of voltage form positive to negative or vice versa
* when voltage changes from negative to positive frequency calculation is started
* else do nothing.
* every time a sample is sampled cycleSamples is incremented by 256
* when voltage is changed from negative to positive check the cycle samples limit
* it has to be in between cycles for min freq and max freq.
* if it is out of limits something has gone wrong and restart the cycle samples
* as we divided the time between 2 samples into 256, now we have to estimate the approximate
* samples it taken before zero crossing
* for that we are doing successive approximation 8 times which give the approx number of cycles
* Finally we are applying a filter to get the stable value
*/
    #ifdef FREQUENCY_SUPPORT
        // Convert cycle samples to floating-point representation
        phase->params.voltagePeriod.cycleSamples += 256;

        /* checking for zero crossing with floating-point tolerance */
        if(fabsf(voltageCorrected - phase->params.lastVoltageSample) <= MAX_PER_SAMPLE_VOLTAGE_SLEW)
        {
            /* This is not a spike. Do mains cycle detection and estimate the precise main period  */
            if(voltageCorrected < 0.0f)
            {
                /* voltage is in negative cycle    */
                phase->status &= ~PHASE_STATUS_V_POS;
            }
            else
            {
                if (!(phase->status & PHASE_STATUS_V_POS))
                {
                    if(phase->params.voltagePeriod.cycleSamples >= METROLOGY_MIN_CYCLE_SAMPLES
                            && phase->params.voltagePeriod.cycleSamples <= METROLOGY_MAX_CYCLE_SAMPLES)
                    {
                        #ifdef SAG_SWELL_SUPPORT
                              int cdp = phase->params.cycleDpSet;
                              phase->params.cycleDotProd[cdp].cycleVoltageSq = 0;
                              phase->params.cycleDotProd[cdp].cycleSampleCount = 0;

                              phase->params.cycleDpSet ^= 1;
                              phase->cycleStatus &= ~PHASE_STATUS_ZERO_CROSSING_MISSED;
                              phase->cycleStatus |= PHASE_STATUS_NEW_LOG;
                        #endif

                        /* Floating-point successive approximation */
                        float diffVoltage = voltageCorrected - phase->params.lastVoltageSample;
                        float approxVoltage = 0.0f;
                        uint32_t samples = 0;
                        int i;
                        for(i = 0; i < 8; i++)
                        {
                            samples <<= 1;
                            diffVoltage *= 0.5f;
                            approxVoltage += diffVoltage;

                            if(approxVoltage > voltageCorrected)
                            {
                                approxVoltage -= diffVoltage;
                            }
                            else
                            {
                                samples |= 1;
                            }
                        }

                        uint32_t z = samples;
                        while(phase->params.sinceLast > 1) {
                            z += samples;
                            phase->params.sinceLast--;
                        }
                        /* Update period with floating-point filtering */
                        phase->params.voltagePeriod.period = phase->params.voltagePeriod.cycleSamples - z;
                        /* start next cycle count */
                        phase->params.voltagePeriod.cycleSamples = z;
                        phaseDP->cycleNumber++;
#ifdef ROGOWSKI_SUPPORT
                        phase->params.voltagePeriod.cyclePeriod =
                            ((phase->params.voltagePeriod.period / 256) / (2 * PI));
                        workingData->currentIntegrationData[ph] = 0.0f;
#endif
                    }
                    else
                    {
                        phase->params.voltagePeriod.cycleSamples = 0;
                        phaseDP->cycleNumber = 0;
                        phase->cycleStatus |= PHASE_STATUS_ZERO_CROSSING_MISSED;
                    }

                    if (phaseDP->cycleNumber >= CYCLES_PER_COMPUTATION)
                    {
                        phLog |= 1;
                        phaseDP->cycleNumber = 0;
                    }
                }
                phase->status |= PHASE_STATUS_V_POS;
            }
            phase->params.sinceLast = 0;
            phase->params.lastVoltageSample = voltageCorrected;
        }
        phase->params.sinceLast++;
    #endif

    if(phaseDP->sampleCount >= SAMPLE_RATE)
    {
        phLog |= 1;
    }

    if(cyclePhaseDP->cycleSampleCount >= TWICE_CYCLES_PER_NOMINAL_FREQUENCY)
    {
        cyclePhaseDP->cycleVoltageSq = 0.0f;
        cyclePhaseDP->cycleSampleCount = 0;
    }

    phLog <<= 1;
    }

    /* if sampling is done its data is logged and used for foreground activity  */
    if(phLog)
    {
        PHASES ph;
        for(ph = PHASE_ONE; ph < MAX_PHASES; ph++)
        {
            if(phLog & (1 << (MAX_PHASES - ph)))
            {
                logPhaseParameters(workingData, ph);
            }
        }
    }

    #ifdef NEUTRAL_MONITOR_SUPPORT
        neutralMetrology *neutral = &workingData->neutral;

        int8_t dp = (neutral->params.dpSet ^ 0x01) & 0x01;
        neutralDP = &neutral->params.dotProd[dp];

        float neutralData = workingData->rawNeutralData;
        neutralCurrent = metrology_dcFilter(&neutral->params.I_dc_estimate, neutral->params.I_History[0]);
        if((neutralCurrent >= I_ADC_MAX || neutralCurrent <= I_ADC_MIN) && workingData->neutral.params.currentEndStops)
        {
            workingData->neutral.params.currentEndStops--;
        }
        neutral->params.I_History[0] = neutral->params.I_History[1];
        neutral->params.I_History[1] = neutralData;
        neutralDP->currentSq += neutralCurrent * neutralCurrent;
        if(++neutralDP->sampleCount >= SAMPLE_RATE)
        {
            logNeutralParameters(workingData);
        }
    #endif
}

/*!
 * @brief Metrology per sample energy pulse processing
 * @param[in] workingData The metrology data
 */
void Metrology_perSampleEnergyPulseProcessing(metrologyData *workingData)
{
    totalParams *total = &workingData->totals;
    float pow;

#ifdef ACTIVE_ENERGY_SUPPORT
    pow = (fabs(workingData->totals.readings.activePower));
    total->energy.activeEnergyPulse.integrator += pow;

    if(total->energy.activeEnergyPulse.integrator >= TOTAL_ACTIVE_ENERGY_PULSE_THRESHOLD)
    {
        total->energy.activeEnergyPulse.integrator -= TOTAL_ACTIVE_ENERGY_PULSE_THRESHOLD;
        total->energy.activeEnergyPulse.pulseRemainingTime = ENERGY_PULSE_DURATION;
        Metrology_startActiveEnergyPulse(workingData);
    }
    if(--total->energy.activeEnergyPulse.pulseRemainingTime == 0.0f)
    {
        Metrology_endActiveEnergyPulse(workingData);
    }
#endif

#ifdef REACTIVE_ENERGY_SUPPORT
    pow = (fabs(workingData->totals.readings.reactivePower));
    total->energy.reactiveEnergyPulse.integrator += pow;

    if(total->energy.reactiveEnergyPulse.integrator >= TOTAL_REACTIVE_ENERGY_PULSE_THRESHOLD)
    {
        total->energy.reactiveEnergyPulse.integrator -= TOTAL_REACTIVE_ENERGY_PULSE_THRESHOLD;
        total->energy.reactiveEnergyPulse.pulseRemainingTime = ENERGY_PULSE_DURATION;
        Metrology_startReactiveEnergyPulse(workingData);
    }
    if(--total->energy.reactiveEnergyPulse.pulseRemainingTime == 0.0f)
    {
        Metrology_endReactiveEnergyPulse(workingData);
    }
#endif
}
