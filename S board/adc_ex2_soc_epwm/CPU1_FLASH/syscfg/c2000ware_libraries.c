/*
 * Copyright (c) 2021 Texas Instruments Incorporated - http://www.ti.com
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
 *
 */

#include "c2000ware_libraries.h"


void C2000Ware_libraries_init()
{
    FFT_init();
    FFR_init();
}

void FFT_init()
{
    myRFFT0_init();
}

RFFT_F32_STRUCT  myRFFT0_obj;
RFFT_F32_STRUCT_Handle myRFFT0_handle = &myRFFT0_obj;

void myRFFT0_init()
{
    RFFT_f32_setInputPtr(myRFFT0_handle, inPtr);
    RFFT_f32_setOutputPtr(myRFFT0_handle, outPtr);
	RFFT_f32_setStages(myRFFT0_handle, myRFFT0_RFFT_NUM_STAGES);
	RFFT_f32_setFFTSize(myRFFT0_handle, myRFFT0_RFFT_SIZE);
    RFFT_f32_setMagnitudePtr(myRFFT0_handle, magPtr);
    RFFT_f32_setPhasePtr(myRFFT0_handle, phPtr);
	RFFT_f32_setTwiddlesPtr(myRFFT0_handle, twiddlePtr);
	RFFT_f32_sincostable(myRFFT0_handle);
}


void FFR_init()
{

}

