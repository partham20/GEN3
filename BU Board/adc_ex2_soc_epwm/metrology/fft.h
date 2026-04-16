/**
 * @file fft.h
 *
 * @brief CMSIS FFT routines header
 *
 * @copyright Copyright (C) 2018 Texas Instruments Incorporated - http://www.ti.com/
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

#ifndef FFT_H_
#define FFT_H_

#include <stdint.h>
#include <stdbool.h>
#include "fpu.h"
#include "dsp.h"
#include "fpu32/fpu_rfft.h"
#include "fpu32/fpu_fft_hann.h"
#include "fpu32/fpu_fft_flattop.h"


#ifdef __cplusplus
extern "C" {
#endif



#ifdef HARMONICS_SUPPORT
// FFT configuration
#define FFT_STAGES          5
#define FFT_SIZE            (1 << FFT_STAGES) // 2048
#define MAX_HARMONIC        50
/**
 * @brief Initialize FFT twiddle factors for harmonic calculation
 *
 * Initializes FFT twiddle factors with Q31 data for harmonic calculation.
 * Length of FFT related to Fs = M/NT
 * where: NT = 200msec (10 cycles at 50Hz or 12 cycles at 60Hz)
 *        Fs = 8 KHz
 *        log2(Fs * NT) = 11
 *        Length = 2^11 = 2048 points
 */
void Init_FFT(void);

/**
 * @brief Find fundamental and harmonics from the input data
 *
 * Based on IEC 61000-4-7, finds the fundamental frequency and harmonics
 * from the input data.
 *
 * @param[in] p_data Pointer to input real data buffer
 * @param[in] index Start index within p_data (for circular buffer handling)
 * @param[out] harmonic_powers Pointer to array where calculated harmonic powers will be stored
 */
void find_harmonics(float *p_data, uint16_t index, float *harmonic_powers);

/*******************************************************************************************************************//**
 *
 * @brief array_safeRange()
 *          Bounds the search in an array to be no greater than the last harmonic and no greater than 
 *          the end of the array 
 *
 * @param[in] arrayLength  	length of input array
 * @param[in] arrayIndex   	index into current array
 * @param[in] spread  		amount to search around
 * @param[out] startIndex  	start search index of the input array
 * @param[out] stopIndex  	stop search index of the input array
 *
 * return none
 */
void array_safeRange( const uint32_t arrayLength, const uint32_t arrayIndex,
					  const uint32_t spread, uint32_t* startIndex, uint32_t* stopIndex );
/*******************************************************************************************************************//**
 *
 * @brief array_indexOfLocalMaxima()
 *          Searches for next local maxima in array starting at startIndex and ending search in stopIndex 
 *
 * @param[in] arrayPtr  	input array
 * @param[in] startIndex   	index to start search
 * @param[in] stopIndex  	index to end search
 *
 * return index of next local maxima
 */
uint32_t array_indexOfLocalMaxima( const float *arrayPtr, const uint32_t startIndex, const uint32_t stopIndex );

/*******************************************************************************************************************//**
 *
 * @brief array_valueExists()
 *          Searches array to see if value exists
 *
 * @param[in] arrayPtr  	input array
 * @param[in] arrayLength   length of input array
 * @param[in] value  		value to match
 *
 * return true if value found in arrayPtr
 */
uint16_t array_valueExists( const uint32_t *arrayPtr, const uint32_t arrayLength, const uint32_t value );

// External variable declarations
// extern CFFT_f64_Handle hnd_cfft;
// RFFT_F32_STRUCT rfft;
// RFFT_F32_STRUCT_Handle hnd_rfft = &rfft;

#endif /* HARMONICS_SUPPORT */

#ifdef __cplusplus
}
#endif

#endif /* FFT_H_ */
