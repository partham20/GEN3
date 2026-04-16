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
#include "metrology_setup.h"
#include "metrology_calibration.h"
#include "template.h"
#include "metrology_calculations.h"

/*!
 * @brief calculate RMS Current
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return RMS Current in amps
 */
float calculateRMSCurrent(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float RMSCurrent;

    if(phase->status & PHASE_STATUS_I_OVERRANGE)
    {
        return RMS_CURRENT_OVERRANGE;
    }

    int8_t dp = phase->params.dpSet;
    currentDPSet *current = &phase->params.current.dotProd[dp];

    if(!current->sampleCount)
    {
        RMSCurrent = 0.0f;
    }
    else
    {
        float sqrms = current->currentSq/(float)current->sampleCount;
        float rms = sqrtf(sqrms);

        RMSCurrent = rms * phaseCal->current.IscaleFactor;

        if(RMSCurrent < phaseCal->current.IacOffset)
        {
            RMSCurrent = 0.0f;
        }
        else
        {
            RMSCurrent -= phaseCal->current.IacOffset;
        }
    }
    return RMSCurrent;
}

/*!
 * @brief calculate dc estimate current
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return  DC Estimate current in amps
 */
float calculateIdcEstimate(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float Idclogged = (float)phase->params.current.I_dc_estimate;
    float IdcEstimate = Idclogged * phaseCal->current.IscaleFactor;
    return IdcEstimate;
}
