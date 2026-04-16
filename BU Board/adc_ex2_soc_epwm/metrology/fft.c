/**
 * @file fft.c
 *
 * @brief CMSIS FFT routines
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
#include <math.h>
#include <string.h>
#include "fpu.h"
#include "dsp.h"
#include "fft.h"
#include "template.h"
#include "metrology_defines.h"


#ifdef HARMONICS_SUPPORT


#pragma DATA_SECTION(FpuRegs,"FpuRegsFile");
volatile struct FPU_REG FpuRegs;

// Object of the structure RFFT_F32_STRUCT
RFFT_F32_STRUCT rfft;
// Handle to the RFFT_F32_STRUCT object
RFFT_F32_STRUCT_Handle hnd_rfft = &rfft;

#pragma DATA_SECTION(test_input, "FFT_buffer_1")
float test_input[FFT_SIZE];

#pragma DATA_SECTION(test_output, "FFT_buffer_2")
float test_output[FFT_SIZE];

float test_magnitude[(FFT_SIZE >> 1) + 1];

const float fftWindow[FFT_SIZE >> 1] = HANN2048;

/*******************************************************************************************************************//**
 *
 * @brief Init_FFT()
 *          Initialize FFT twiddle factors for float data for harmonic calculation
 *          	Length of FFT related to Fs = M/NT
 *          	where: NT = 200msec (10 cycles at 50Hz or 12 cycles at 60Hz)
 *          		Fs = 8 KHz
 *          		log2(Fs * NT) = 11
 *          		Length = 2^11 = 2048 points
 *
 */
void  Init_FFT( void )
{
	// Configure the object
    RFFT_f32_setInputPtr(hnd_rfft, test_input);
    RFFT_f32_setOutputPtr(hnd_rfft, test_output);
    RFFT_f32_setMagnitudePtr(hnd_rfft, test_magnitude);
    RFFT_f32_setTwiddlesPtr(hnd_rfft, RFFT_f32_twiddleFactors);
    RFFT_f32_setStages(hnd_rfft, (FFT_STAGES));
    RFFT_f32_setFFTSize(hnd_rfft, (FFT_SIZE));
}


/*******************************************************************************************************************//**
 *
 * @brief find_harmonics()
 *          Find fundametal and harmonics from the input data
 *          Based on IEC 61000-4-7
 *
 * @param[in,out] 	p_data  pointer to input real data and in place replacement with magnitude data
 * @param[out] 		harmonic_bins   pointer to harmonics data
 * @param[out] 		fundamental_mag pointer to store the fundamental magnitude
 *
 * return filtered output
 */


/* 
    Inputs:
        float *p_data: pointer to input real data array. This array is modified in-place to store magnitude data after processing.
        int8_t index: Start index of the input data
        float *harmonic_powers: pointer to an array that will store harmonic powers.
 */
void find_harmonics( float *p_data, uint16_t index, float *harmonic_powers )
{
    uint32_t fundamental_index, i, j, k;
    int16_t harmonic_index_coarse, skew = 0;
    uint32_t start, stop;
	const uint32_t binSpread = 1;
    float *p_temp;
    uint32_t harmonic_bins[MAX_HARMONIC];

    const uint32_t fft_mag_size = (FFT_SIZE >> 1); // Max valid index in magnitude array (excluding DC)
    // IEC 61000-4-7 specifies grouping based on 10 cycles (50Hz) or 12 cycles (60Hz) in 200ms.
    // Assuming CYCLES_PER_COMPUTATION is defined correctly (e.g., 10 or 12).
    // Group half-width Delta_f = 5 Hz for 200ms window. Bin width = Fs/N = 8000/2048 = 3.90625 Hz.
    // Number of bins = Delta_f / Bin_width = 5 / 3.90625 = 1.28.
    // The standard often uses a fixed group width (e.g., 5 bins total -> half_width = 2) or relates it to cycles.
    // Let's stick with CYCLES_PER_COMPUTATION / 2 for now as per the original code's likely intent.
    const int group_half_width = (CYCLES_PER_COMPUTATION / 2); // Bins to include on each side for grouping

    memset(test_input, 0U, sizeof(float)*FFT_SIZE);
    memset(test_output, 0U, sizeof(float)*FFT_SIZE);
    memset(test_magnitude, 0U, sizeof(float)*(FFT_SIZE>>1 + 1));


    // Copy input data to the dedicated buffer in FFT_buffer_1 section
     memcpy(test_input, &p_data[index], sizeof(float) * (FFT_SIZE - index)); // Copy the block from the oldest sample to the end of the circular buffer into the beginning of test_input.

     if(index != 0)
         memcpy(&test_input[FFT_SIZE - index], p_data, sizeof(float) * index); // Copy the block from the beginning of the circular buffer to the end of test_input.

    // Apply the window
    RFFT_f32_win(&test_input[0], (float *)&fftWindow, FFT_SIZE);

	// Run the N point real FFT
    RFFT_f32(hnd_rfft);

	// Run the magnitude function
    RFFT_f32_mag_TMU0(hnd_rfft);

    p_temp = test_magnitude;

    // Find the maximum value in the magnitude array (Fundamental)
    float max_magnitude = 0.0f;
    uint32_t max_index = 1; // Start search from index 1 (skip DC)

    // Search up to half the FFT size (Nyquist limit)
    for(j=1U; j <= fft_mag_size ; j++)
    {
        if (p_temp[j] > max_magnitude) {
            max_magnitude = p_temp[j];
            max_index = j;
        }
    }
    fundamental_index = max_index;

    // --- Find Harmonic Bin Locations ---
    // Store the fundamental bin index first (used as reference)
    harmonic_bins[0] = fundamental_index;
    skew = 0; // Reset skew for harmonic search

    // Look for harmonics from 2nd harmonic (i=1) up to MAX_HARMONIC-1
    for ( i = 1; i < MAX_HARMONIC; i++ ) {
		//  Find coarse bin index for harmonic h = (i+1)
    	harmonic_index_coarse =  ((i + 1) * fundamental_index) + skew;

        // Ensure coarse index is within valid range before searching
        if (harmonic_index_coarse < 1) harmonic_index_coarse = 1;
        if (harmonic_index_coarse > fft_mag_size) harmonic_index_coarse = fft_mag_size;

    	// Find a more precise bin index by searching for a local maxima around the coarse index
		array_safeRange( fft_mag_size + 1, harmonic_index_coarse, binSpread, &start, &stop ); // Use fft_mag_size+1 for length

        // Ensure start/stop are within valid magnitude data range [1, fft_mag_size]
        if (start == 0) start = 1; // Don't include DC bin 0 in maxima search
        if (stop > fft_mag_size) stop = fft_mag_size;
        if (start > stop) start = stop; // Handle edge case if spread pushes start beyond stop

		const uint32_t maximaIndex = array_indexOfLocalMaxima( p_temp, start, stop );

		//  Check to see if maxima index already exists in harmonics[] array (i.e. tones overlap)...
		//	If it does, then use coarse index instead of maximaIndex to avoid double counting.
        //  Store the found/adjusted index for harmonic h = (i+1).
		if ( array_valueExists( harmonic_bins, i, maximaIndex ) ) { // Check up to index i (already found harmonics)
			harmonic_bins[i] = harmonic_index_coarse;
            // Don't adjust skew if we reverted to coarse index due to overlap
		} else {
			harmonic_bins[i] = maximaIndex;
			// Adjust skew based on the difference between expected and actual maxima
            // Only update skew if the maximaIndex is reasonably close to coarse index? (Optional refinement)
            skew += (int16_t)(maximaIndex - harmonic_index_coarse);
		}

        // Ensure harmonic bin is within valid range [1, fft_mag_size]
        if (harmonic_bins[i] < 1) harmonic_bins[i] = 1;
        if (harmonic_bins[i] > fft_mag_size) harmonic_bins[i] = fft_mag_size;
    }

    // --- Calculate Harmonic Powers (Square Magnitudes and Group) ---
    memset(harmonic_powers, 0, sizeof(float) * MAX_HARMONIC); // Clear output array

    // Fundamental Power (harmonic_powers[0]): Grouped power sum around fundamental_index
    // Use group_half_width for fundamental grouping as well.
    uint32_t center_bin_idx = harmonic_bins[0]; // fundamental_index
    float fundamental_power_sum = 0.0f;

    // Sum power of the center bin
    if (center_bin_idx >= 1 && center_bin_idx <= fft_mag_size) {
         fundamental_power_sum += (p_temp[center_bin_idx] * p_temp[center_bin_idx]);
    }
    // Add power from adjacent bins
    for ( k = 1; k <= group_half_width; k++ ) {
        // Bin below center
        int current_bin_index_neg = center_bin_idx - k;
        if (current_bin_index_neg >= 1 && current_bin_index_neg <= fft_mag_size) {
             float power = p_temp[current_bin_index_neg] * p_temp[current_bin_index_neg];
             if (k == group_half_width) {
                fundamental_power_sum += (power / 4.0f); // IEC edge bin correction (Power/4)
             } else {
                fundamental_power_sum += power;
             }
        }
        // Bin above center
        int current_bin_index_pos = center_bin_idx + k;
         if (current_bin_index_pos >= 1 && current_bin_index_pos <= fft_mag_size) {
             float power = p_temp[current_bin_index_pos] * p_temp[current_bin_index_pos];
             if (k == group_half_width) {
                fundamental_power_sum += (power / 4.0f); // IEC edge bin correction (Power/4)
             } else {
                fundamental_power_sum += power;
             }
        }
    }
    harmonic_powers[0] = fundamental_power_sum;


    // 2nd Harmonic Power (harmonic_powers[1]): Power of the single bin found for the 2nd harmonic
    // IEC 61000-4-7 often treats the 2nd harmonic without grouping or with minimal grouping.
    // Let's keep the single bin power for simplicity, matching the previous logic.
    if (MAX_HARMONIC > 1) {
        uint32_t bin_idx = harmonic_bins[1];
        if (bin_idx >= 1 && bin_idx <= fft_mag_size) {
            harmonic_powers[1] = p_temp[bin_idx] * p_temp[bin_idx];
        } else {
            harmonic_powers[1] = 0.0f; // Bin index out of range
        }
    }

    // 3rd Harmonic and higher Powers (harmonic_powers[i] for i >= 2): Grouped power sum
    for ( i = 2; i < MAX_HARMONIC; i++ ) {
        float harmonic_power_sum = 0.0f;
        center_bin_idx = harmonic_bins[i];

        // Sum power of the center bin
        if (center_bin_idx >= 1 && center_bin_idx <= fft_mag_size) {
             harmonic_power_sum += (p_temp[center_bin_idx] * p_temp[center_bin_idx]);
        }

        // Add power from adjacent bins based on IEC grouping (CYCLES_PER_COMPUTATION determines width)
        for ( k = 1; k <= group_half_width; k++ ) {
             // Bin below center
            int current_bin_index_neg = center_bin_idx - k;
            if (current_bin_index_neg >= 1 && current_bin_index_neg <= fft_mag_size) { // Check bounds
                 float power = p_temp[current_bin_index_neg] * p_temp[current_bin_index_neg];
                 if (k == group_half_width) {
                    harmonic_power_sum += (power / 4.0f); // IEC edge bin correction (Power/4)
                 } else {
                    harmonic_power_sum += power;
                 }
            }
             // Bin above center
            int current_bin_index_pos = center_bin_idx + k;
            if (current_bin_index_pos >= 1 && current_bin_index_pos <= fft_mag_size) { // Check bounds
                 float power = p_temp[current_bin_index_pos] * p_temp[current_bin_index_pos];
                 if (k == group_half_width) {
                    harmonic_power_sum += (power / 4.0f); // IEC edge bin correction (Power/4)
                 } else {
                    harmonic_power_sum += power;
                 }
            }
        }
        harmonic_powers[i] = harmonic_power_sum;
    }

}

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
					  const uint32_t spread, uint32_t* startIndex, uint32_t* stopIndex )
{
    // Ensure arrayLength is at least 1 to avoid issues with arrayLength - 1 below
    if (arrayLength == 0) {
        *startIndex = 0;
        *stopIndex = 0;
        return;
    }

	// Calculate potential range boundaries
    // Use subtraction check to prevent underflow if arrayIndex < spread
	const uint32_t rangeMin = (arrayIndex > spread) ? (arrayIndex - spread) : 0;
	const uint32_t rangeMax = arrayIndex + spread; // Overflow check needed below

	// Determine start index
	*startIndex = rangeMin; // Already handled underflow

	// Determine stop index -> Set to (arrayLength - 1) if (arrayIndex + spread) exceeds or equals arrayLength
    // Also handle potential overflow of rangeMax calculation if arrayIndex + spread is very large
	*stopIndex = (rangeMax < arrayLength && rangeMax >= arrayIndex) ? rangeMax : (arrayLength - 1);

    // Final check: ensure startIndex is not greater than stopIndex
    if (*startIndex > *stopIndex) {
        // This might happen if arrayIndex itself is >= arrayLength initially,
        // or due to rounding/edge cases. Set to a valid single point or empty range.
        // Setting start to stop seems reasonable if stop is valid.
         if (*stopIndex < arrayLength) {
             *startIndex = *stopIndex;
         } else {
             // If stopIndex is also invalid, maybe set both to arrayLength-1 or 0?
             // Setting to arrayLength-1 is safer if arrayLength > 0.
             *startIndex = (arrayLength > 0) ? arrayLength - 1 : 0;
             *stopIndex = *startIndex;
         }
    }
}

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
uint32_t array_indexOfLocalMaxima( const float *arrayPtr, const uint32_t startIndex, const uint32_t stopIndex )
{
	// Pick startIndex as our initial maxima
	float tempMax = arrayPtr[startIndex];
	uint32_t indexOfMaxima = startIndex;
	uint32_t index;

	// Search between start and stop bins for a local maxima...
	for ( index = startIndex + 1; index <= stopIndex; ++index ) {
		// Check if value is larger...
		if ( arrayPtr[index] > tempMax ) {
			indexOfMaxima = index;
			tempMax = arrayPtr[index];
		}
	}
	
	return indexOfMaxima;
}

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
uint16_t array_valueExists( const uint32_t *arrayPtr, const uint32_t arrayLength, const uint32_t value )
{
	uint16_t i;

	// Loop through array and return true as soon as a match is found
	for ( i = 0; i < arrayLength; ++i ) {
		if ( value == arrayPtr[i] ) { 
			return 1; 
		}
	}

	// No match found, return false
	return 0;
}

#endif
