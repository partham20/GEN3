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
#include "metrology_defines.h"
#include "metrology_structs.h"
#include "metrology_calibration.h"
#include "metrology_setup.h"

/*!
 * @brief Metrology Init for RMS Current
 * @param[in] workingData The metrology data
 */
void Metrology_init(metrologyData *workingData)
{
    Metrology_initializeCalibrationData();

    int ph;
    for(ph = 0; ph < MAX_PHASES; ph++)
    {
        phaseMetrology *phase = &workingData->phases[ph];
        phaseCalibrationData *phaseCal = &calInfo->phases[ph];

        /* Initialize current DC estimate */
        phase->params.current.I_dc_estimate = (phaseCal->current.IinitialDcEstimate / phaseCal->current.IscaleFactor);
        phase->params.current.I_dc_estimate_logged = 0.0f;

        /* Initialize current end stops for overrange detection */
        phase->params.current.currentEndStops = ENDSTOP_HITS_FOR_OVERLOAD;

        /* Initialize dot product sets */
        phase->params.current.dotProd[0].currentSq = 0.0f;
        phase->params.current.dotProd[0].sampleCount = 0;
        phase->params.current.dotProd[1].currentSq = 0.0f;
        phase->params.current.dotProd[1].sampleCount = 0;

        /* Initialize history buffer */
        phase->params.current.IHistory[0] = 0.0f;
        phase->params.current.IHistory[1] = 0.0f;

        /* Initialize dpSet index */
        phase->params.dpSet = 0;
        phase->params.current.dpSet = 0;

        /* Initialize status */
        phase->status = 0;

        /* Initialize readings */
        phase->readings.RMSCurrent = 0.0f;
    }

    workingData->operatingMode = OPERATING_MODE_NORMAL;
    workingData->metrologyState = METROLOGY_STATUS_PHASE_VOLTAGE_OK;
}
