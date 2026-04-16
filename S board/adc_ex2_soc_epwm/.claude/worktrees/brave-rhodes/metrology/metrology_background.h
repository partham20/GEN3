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

/*!****************************************************************************
 *  @file       metrology_background.h
 *  @brief      API's related to metrology background process
 *
 *  @anchor metrology_background_h
 *  # Overview
 *
 *  API's related to background process in metrology module
 *
 *  <hr>
 ******************************************************************************/
/** @addtogroup Metrology
 * @{
 */

#ifndef _METROLOGY_BACKGROUND_H_
#define _METROLOGY_BACKGROUND_H_

#include "inc/hw_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/*!
 * @brief Metrology per phase sampling
 * @param[in] workingData The metrology data
 */
void Metrology_perSampleProcessing(metrologyData *workingData);

/*!
 * @brief Metrology per sample energy pulse processing
 * @param[in] workingData The metrology data
 */
void Metrology_perSampleEnergyPulseProcessing(metrologyData *workingData);

/*!
 * @brief Startup sample counter for two-speed DC filter.
 *        During the first SAMPLE_RATE samples (~1 second), a faster
 *        coefficient (1/256) is used so the DC estimate converges quickly.
 *        After that, the normal slow coefficient (1/16384) is used for
 *        accurate steady-state DC removal.
 */
extern volatile uint32_t g_dcFilterStartupCount;

/*!
 * @brief dc_filter()
 *          Simple 1st order DC filter (floating-point version)
 *              y(n)     = y(n-1) + (x(n) - y(n-1))/16384.0
 *              xfilt(n) = x(n) - y(n)
 *   where:
 *    x(n)     = most recent ADC sample
 *    y(n)     = filter state
 *    xfilt(n) = DC filtered output
 *
 * @param[in,out]   state  Pointer to filter state
 * @param[in]       x      Input sample
 *
 * @return filtered output
 */
static inline float metrology_dcFilter(float *state, float x)
{
    static const float FILTER_COEFF_FAST = 0.00390625f;        // 1/256
    static const float FILTER_COEFF_SLOW = 0.000061035156250f; // 1/16384

    float coeff = (g_dcFilterStartupCount < SAMPLE_RATE) ?
                  FILTER_COEFF_FAST : FILTER_COEFF_SLOW;

    /* Apply the filter: y(n) = y(n-1) + (x(n) - y(n-1)) * FILTER_COEFF */
    *state += (x - *state) * coeff;

    /* Calculate DC-filtered output: xfilt(n) = x(n) - y(n) */
    return x - *state;
}

#ifdef __cplusplus
}
#endif
#endif /* _METROLOGY_BACKGROUND_H_ */
/** @}*/
