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
// #include <cstdint>
#include <stdint.h>
#include "inc/hw_types.h"
#include "metrology_defines.h"
#include "metrology_structs.h"
#include "metrology_setup.h"
#include "metrology_calibration.h"
#include "template.h"
#include <fft.h>
#include "metrology_calculations.h"

//*****************************************************************************
// Global Functions
//*****************************************************************************
/*!
 * @brief Calculate RMS Voltage from accumulated voltage squared samples
 * @param[in] phase    The phase metrology containing raw sample data
 * @param[in] phaseCal The phase calibration data with scaling factors
 * @return  RMS Voltage in volts
 *
 * This function processes the accumulated voltage squared samples to calculate
 * the true RMS voltage value. It applies the voltage scale factor and offset
 * corrections based on calibration data. Out-of-range conditions are handled
 * by returning a defined overrange value.
 */
float Gain_THD=1.0f;
float calculateRMSVoltage(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float RMSVoltage;

    if(phase->status & PHASE_STATUS_V_OVERRANGE)
    {
        /* input voltage is out of range    */
        return RMS_VOLTAGE_OVERRANGE;
    }

    /* index of the dot product currently set for foreground processing */
    int8_t dp = phase->params.dpSet;
    voltageDPSet *voltage = &phase->params.dotProd[dp];

    if(!voltage->sampleCount)
    {
        /* Number of samples is zero    */
        RMSVoltage = 0.0f;
    }
    else
    {
        /* Calculate mean square value by dividing accumulated squared samples by count */
        float sqrms = (voltage->voltageSq/(float)voltage->sampleCount);
        /* Take square root to get true RMS value */
        float rms = sqrtf(sqrms);

        /* Apply calibration scale factor */
        RMSVoltage = (rms * (phaseCal->VscaleFactor));

        /* Apply offset correction if value is above offset threshold */
        if(RMSVoltage < phaseCal->VacOffset)
        {
            /* RMS voltage is smaller than offset error */
            RMSVoltage = 0.0f;
        }
        else
        {
            RMSVoltage -= phaseCal->VacOffset;
        }
    }
    return RMSVoltage;
}

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
        /* Input current is out of range    */
        return RMS_CURRENT_OVERRANGE;
    }

    /* index of the dot product currently set for foreground processing */
    int8_t dp = phase->params.dpSet;
    currentDPSet *current = &phase->params.current.dotProd[dp];

    if(!current->sampleCount)
    {
        /* Number of samples is zero    */
        RMSCurrent = 0.0f;
    }
    else
    {
        float sqrms = current->currentSq/(float)current->sampleCount;
        float rms = sqrtf(sqrms);

        RMSCurrent = rms * phaseCal->current.IscaleFactor;

        if(RMSCurrent < phaseCal->current.IacOffset)
        {
            /* RMS current is smaller than offset error */
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
 * @brief calculate active power
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return Active Power in watts (IQ13 format)
 */
float calculateActivePower(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float activePower;

    if(phase->status & (PHASE_STATUS_V_OVERRANGE | PHASE_STATUS_I_OVERRANGE))
    {
        /* Input voltage or current is out of range */
        return POWER_OVERRANGE;
    }

    /* index of the dot product currently set for foreground processing */
    int8_t dp = phase->params.dpSet;
    currentDPSet *current = &phase->params.current.dotProd[dp];

    if(!current->sampleCount)
    {
        /* Number of samples is zero    */
        activePower = 0.0f;
    }
    else
    {
        float actpower = current->activePower/(float)current->sampleCount;
        float scaling = (phaseCal->current.PscaleFactor * phase->params.current.phaseCorrection.firGain);
        activePower = (actpower * scaling);
    }

    if (activePower > phaseCal->current.activePowerOffset)
    {
        activePower-= phaseCal->current.activePowerOffset;
    }
    return activePower;
}

/*!
 * @brief calculate active power
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return Reactive Power in VAR (IQ13 format)
 */
float calculateReactivePower(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float reactivePower;

    if(phase->status & (PHASE_STATUS_V_OVERRANGE | PHASE_STATUS_I_OVERRANGE))
    {
        /* Input voltage or current is out of range */
        return POWER_OVERRANGE;
    }

    /* index of the dot product currently set for foreground processing */
    int8_t dp = phase->params.dpSet;
    currentDPSet *current = &phase->params.current.dotProd[dp];

    if(!current->sampleCount)
    {
        /* Number of samples is zero    */
        reactivePower = 0.0f;
    }
    else
    {
        float reactpower = current->reactivePower/(float)current->sampleCount;
        float scaling = (phaseCal->current.PscaleFactor * phase->params.current.quadratureCorrection.firGain);
        reactivePower = (reactpower * scaling);
    }

    if (reactivePower > phaseCal->current.reactivePowerOffset)
    {
        reactivePower-= phaseCal->current.reactivePowerOffset;
    }
    return reactivePower;
}

/*!
 * @brief calculate apparent power
 * @param[in] phase    The phase metrology
 * @return Apparent Power in VA (IQ13 format)
 */
float calculateApparentPower(phaseMetrology *phase)
{
    return hypotf(phase->readings.activePower, phase->readings.reactivePower);
}

/*!
 * @brief calculate fundamental RMS Voltage
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return Fundamental RMS Voltage in volts
 */
float calculateFundamentalRMSVoltage(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float FRMSVoltage;

    if(phase->status & PHASE_STATUS_V_OVERRANGE)
    {
        return RMS_VOLTAGE_OVERRANGE;
    }

    /* index of the dot product currently set for foreground processing */
    int8_t dp = phase->params.dpSet;
    voltageDPSet *voltage = &phase->params.dotProd[dp];

    if(!phase->params.dotProd[dp].sampleCount)
    {
        FRMSVoltage = 0.0f;
    }
    else
    {
        float fVoltageI = voltage->fVoltageSq /(float)voltage->sampleCount;
        float fVoltageQ = voltage->fVoltageQuardSq /(float)voltage->sampleCount;
        float fVoltage = sqrtf(fVoltageI * fVoltageI + fVoltageQ * fVoltageQ);
        float scaling = (phaseCal->VscaleFactor * phase->params.current.phaseCorrection.firGain);
        FRMSVoltage = (fVoltage * scaling * 2.0f); // Multiply by 2 to compensate for sin correlation
        FRMSVoltage /= sqrtf(2.0f);
        if (FRMSVoltage < phaseCal->VFundamentalAcOffset)
        {
            FRMSVoltage = 0.0f;
        }
        else
        {
            FRMSVoltage -= phaseCal->VFundamentalAcOffset;
        }
    }

    if(FRMSVoltage < 0.0f)
    {
        FRMSVoltage = 0.0f;
    }
    return FRMSVoltage;
}

/*!
 * @brief calculate fundamental RMS Current
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return Fundamental RMS Current in amps
 */
float calculateFundamentalRMSCurrent(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float FRMSCurrent;

    if(phase->status & PHASE_STATUS_I_OVERRANGE)
    {
        return RMS_CURRENT_OVERRANGE;
    }

    /* avoid dividing by 0 and negative value voltages */
    if(phase->readings.FRMSVoltage <= 0.0f)
    {
        FRMSCurrent = 0.0f;
    }
    else
    {
        /* Fundamental RMS current calculation */
        FRMSCurrent = ((phase->readings.FApparentPower) / (phase->readings.FRMSVoltage ));

        /* Calibration */
        if(FRMSCurrent < phaseCal->current.IFAcOffset)
        {
            FRMSCurrent = 0.0f;
        }
        else
        {
            FRMSCurrent -= phaseCal->current.IFAcOffset;
        }
    }
    return FRMSCurrent;
}

float calculateFundamentalActivePower(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float FActivePower;

    if(phase->status & (PHASE_STATUS_V_OVERRANGE | PHASE_STATUS_I_OVERRANGE))
    {
        return POWER_OVERRANGE;
    }

    int8_t dp = phase->params.dpSet;
    currentDPSet *current = &phase->params.current.dotProd[dp];

    if(!current->sampleCount)
    {
        FActivePower = 0.0f;
    }
    else
    {
        float factpower = current->FActivePower /(float)current->sampleCount;

        float fVoltageI = phase->params.dotProd[dp].fVoltageSq /(float)phase->params.current.dotProd[dp].sampleCount;
        float fVoltageQ = phase->params.dotProd[dp].fVoltageQuardSq /(float)phase->params.current.dotProd[dp].sampleCount;
        float fVoltage = sqrtf(fVoltageI * fVoltageI + fVoltageQ * fVoltageQ);

        float scaling = (fVoltage * phaseCal->current.PscaleFactor);
        scaling = (scaling * phase->params.current.phaseCorrection.firGain);

        FActivePower = (factpower * scaling * 2.0f);
    }
    return FActivePower;
}

/*!
 * @brief calculate fundamental reactive Power
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return Fundamental Reactive Power in VAR (IQ13 format)
 */
float calculateFundamentalReactivePower(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float FReactivePower;

    if(phase->status & (PHASE_STATUS_V_OVERRANGE | PHASE_STATUS_I_OVERRANGE))
    {
        return POWER_OVERRANGE;
    }

    int8_t dp = phase->params.dpSet;
    currentDPSet *current = &phase->params.current.dotProd[dp];

    if(!current->sampleCount)
    {
        FReactivePower = 0.0f;
    }
    else
    {
       float freactpower = current->FReactivePower/(float)current->sampleCount;
       float fVoltageI = phase->params.dotProd[dp].fVoltageSq /(float)phase->params.current.dotProd[dp].sampleCount;
       float fVoltageQ = phase->params.dotProd[dp].fVoltageQuardSq /(float)phase->params.current.dotProd[dp].sampleCount;
       float fVoltage = sqrtf(fVoltageI * fVoltageI + fVoltageQ * fVoltageQ);
       float scaling = (fVoltage * phaseCal->current.PscaleFactor);
       scaling = (scaling * phase->params.current.quadratureCorrection.firGain);
       FReactivePower = (freactpower * scaling * 2.0f);
    }
    return FReactivePower;
}

/*!
 * @brief calculate fundamental apparent Power
 * @param[in] phase    The phase metrology
 * @return Fundamental Apparent Power in VA (IQ13 format)
 */
float calculateFundamentalApparentPower(phaseMetrology *phase)
{
    return hypotf(phase->readings.FActivePower, phase->readings.FReactivePower);
}

/*!
 * @brief accumulate active energy
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 */
void accumulateActiveEnergy(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];
    totalParams *total = &workingData->totals;
    int8_t dp = phase->params.dpSet;
    volatile float energy;

    float power = fabsf(phase->readings.activePower);
    if(phase->readings.activePower >= 0.0)
    {
        energy = integratePowertoEnergy(&phase->energy.activeEnergyImported, power,
                                       phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.activeEnergyImported.energy += energy;
            total->energy.activeEnergyImported.energy += energy;
        }
    }
    else
    {
        energy = integratePowertoEnergy(&phase->energy.activeEnergyExported, power,
                                       phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.activeEnergyExported.energy += energy;
            total->energy.activeEnergyExported.energy += energy;
        }
    }
}

/*!
 * @brief accumulate reactive energy
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 */
void accumulateReactiveEnergy(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];
    totalParams *total = &workingData->totals;
    int8_t dp = phase->params.dpSet;
    volatile float energy;

    float power = fabsf(phase->readings.reactivePower);
    if(phase->readings.reactivePower >= 0.0f)
    {
        if(phase->readings.activePower >= 0.0f)
        {
            energy = integratePowertoEnergy(&phase->energy.reactiveEnergyQuardrantI, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.reactiveEnergyQuardrantI.energy += energy;
                total->energy.reactiveEnergyQuardrantI.energy += energy;
            }
        }
        else
        {
            energy = integratePowertoEnergy(&phase->energy.reactiveEnergyQuardrantII, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.reactiveEnergyQuardrantII.energy += energy;
                total->energy.reactiveEnergyQuardrantII.energy += energy;
            }
        }
    }
    else
    {
        if(phase->readings.activePower >= 0.0f)
        {
            energy = integratePowertoEnergy(&phase->energy.reactiveEnergyQuardrantIV, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.reactiveEnergyQuardrantIV.energy += energy;
                total->energy.reactiveEnergyQuardrantIV.energy += energy;
            }
        }
        else
        {
            energy = integratePowertoEnergy(&phase->energy.reactiveEnergyQuardrantIII, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.reactiveEnergyQuardrantIII.energy += energy;
                total->energy.reactiveEnergyQuardrantIII.energy += energy;
            }
        }
    }
}

/*!
 * @brief accumulate appparent energy
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 */
void accumulateApparentEnergy(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];
    totalParams *total = &workingData->totals;
    int8_t dp = phase->params.dpSet;
    volatile float energy;

    float power = fabsf(phase->readings.apparentPower);
    if(phase->readings.activePower >= 0.0f)
    {
        energy = integratePowertoEnergy(&phase->energy.appparentEnergyImported, power, phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.appparentEnergyImported.energy += energy;
            total->energy.appparentEnergyImported.energy += energy;
        }
    }
    else
    {
        energy = integratePowertoEnergy(&phase->energy.appparentEnergyExported, power, phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.appparentEnergyExported.energy += energy;
            total->energy.appparentEnergyExported.energy += energy;
        }
    }
}

/*!
 * @brief accumulate fundamental active energy
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 */
void accumulateFundamentalActiveEnergy(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];
    totalParams *total = &workingData->totals;
    int8_t dp = phase->params.dpSet;
    volatile float energy;

    float power = fabsf(phase->readings.FActivePower);
    if(phase->readings.FActivePower >= 0.0f)
    {
        energy = integratePowertoEnergy(&phase->energy.fundamentalActiveEnergyImported, power, phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.fundamentalActiveEnergyImported.energy += energy;
            total->energy.fundamentalActiveEnergyImported.energy += energy;
        }
    }
    else
    {
        energy = integratePowertoEnergy(&phase->energy.fundamentalActiveEnergyExported, power, phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.fundamentalActiveEnergyExported.energy += energy;
            total->energy.fundamentalActiveEnergyExported.energy += energy;
        }
    }
}

/*!
 * @brief accumulate fundamental reactive energy
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 */
void accumulateFundamentalReactiveEnergy(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];
    totalParams *total = &workingData->totals;
    int8_t dp = phase->params.dpSet;
    volatile float energy;

    float power = fabsf(phase->readings.FReactivePower);
    if(phase->readings.FReactivePower >= 0.0f)
    {
        if(phase->readings.activePower >= 0.0f)
        {
            energy = integratePowertoEnergy(&phase->energy.fundamentalReactiveEnergyQuardrantI, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.fundamentalReactiveEnergyQuardrantI.energy += energy;
                total->energy.fundamentalReactiveEnergyQuardrantI.energy += energy;
            }
        }
        else
        {
            energy = integratePowertoEnergy(&phase->energy.fundamentalReactiveEnergyQuardrantII, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.fundamentalReactiveEnergyQuardrantII.energy += energy;
                total->energy.fundamentalReactiveEnergyQuardrantII.energy += energy;
            }
        }
    }
    else
    {
        if(phase->readings.activePower >= 0.0f)
        {
            energy = integratePowertoEnergy(&phase->energy.fundamentalReactiveEnergyQuardrantIV, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.fundamentalReactiveEnergyQuardrantIV.energy += energy;
                total->energy.fundamentalReactiveEnergyQuardrantIV.energy += energy;
            }
        }
        else
        {
            energy = integratePowertoEnergy(&phase->energy.fundamentalReactiveEnergyQuardrantIII, power, phase->params.current.dotProd[dp].sampleCount);
            if(energy)
            {
                phase->energy.fundamentalReactiveEnergyQuardrantIII.energy += energy;
                total->energy.fundamentalReactiveEnergyQuardrantIII.energy += energy;
            }
        }
    }
}

/*!
 * @brief accumulate fundamental apparent energy
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 */
void accumulateFundamentalApparentEnergy(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];
    totalParams *total = &workingData->totals;
    int8_t dp = phase->params.dpSet;
    volatile float energy;

    float power = fabsf(phase->readings.FApparentPower);
    if(phase->readings.FApparentPower >= 0.0f)
    {
        energy = integratePowertoEnergy(&phase->energy.fundamentalAppparentEnergyImported, power, phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.fundamentalAppparentEnergyImported.energy += energy;
            total->energy.fundamentalAppparentEnergyImported.energy += energy;
        }
    }
    else
    {
        energy = integratePowertoEnergy(&phase->energy.fundamentalAppparentEnergyExported, power, phase->params.current.dotProd[dp].sampleCount);
        if(energy)
        {
            phase->energy.fundamentalAppparentEnergyExported.energy += energy;
            total->energy.fundamentalAppparentEnergyExported.energy += energy;
        }
    }
}

/*!
 * @brief Calculate power factor from active and apparent power
 * @param[in] phase  The phase metrology information
 * @return Power factor as a signed value between -1.0 and 1.0
 *
 * This function calculates the power factor as the ratio between active power
 * and apparent power, with sign conventions according to IEEE standards:
 * - Positive value: Inductive load (current lags voltage)
 * - Negative value: Capacitive load (current leads voltage)
 * - Sign is also affected by power flow direction (import/export)
 *
 * Note that values close to zero may have reduced accuracy due to
 * division with small apparent power values.
 */
float calculatePowerFactor(phaseMetrology *phase)
{
    /* Calculate basic power factor value as ratio of active to apparent power */
    float powerFactor = phase->readings.activePower / phase->readings.apparentPower;

    /* Adjust sign based on reactive power quadrant */
    if(phase->readings.reactivePower > 0.0f)
    {
        /* Reactive power is positive (inductive) */
        powerFactor = -powerFactor;
    }

    /* Adjust sign based on power flow direction */
    if(phase->readings.activePower < 0.0f)
    {
        /* Active power is negative (power export) */
        powerFactor = -powerFactor;
    }

    return powerFactor;
}

/*!
 * @brief calculate power factor angle
 * @param[in] phase  The phase metrology
 * @return Power factor angle in per unit with base value of 180 degrees(3.14 radians)
 *         180 degrees corresponds to 1
 *         -180 degrees corresponds to -1
 */
float calculateAngleVoltagetoCurrent(phaseMetrology *phase)
{
    float angle = atan2f(phase->readings.reactivePower, phase->readings.activePower);
    angle =  (angle / (PI));
    return angle;
}

/*!
 * @brief calculate voltage over deviation
 * @param[in] phase  The phase metrology
 * @return Voltage over deviation in percentage
 */
float calculateOverDeviation(phaseMetrology *phase)
{
    float overDeviation;
    if(phase->readings.RMSVoltage < MAINS_NOMINAL_VOLTAGE_DBL)
    {
        overDeviation = 0.0f;
    }
    else
    {
        overDeviation = ((phase->readings.RMSVoltage - MAINS_NOMINAL_VOLTAGE_DBL) / MAINS_NOMINAL_VOLTAGE_DBL);
    }
    return overDeviation * 100.0f;
}

/*!
 * @brief calculate voltage under deviation
 * @param[in] phase  The phase metrology
 * @return Voltage under deviation in percentage
 */
float calculateUnderDeviation(phaseMetrology *phase)
{
    float underDeviation;
    if(phase->readings.RMSVoltage > MAINS_NOMINAL_VOLTAGE_DBL)
    {
        underDeviation = 0.0f;
    }
    else
    {
        underDeviation = ((MAINS_NOMINAL_VOLTAGE_DBL - phase->readings.RMSVoltage) / MAINS_NOMINAL_VOLTAGE_DBL);
    }
    return underDeviation * 100.0f;
}

/*!
 * @brief calculate voltage THD
 * @param[in] phase  The phase metrology
 * @return Voltage THD in percentage
 */
float calculateVoltageTHD(phaseMetrology *phase)
{
    float VRMS2 = phase->readings.RMSVoltage * phase->readings.RMSVoltage;
    float VFRMS2 = phase->readings.FRMSVoltage * phase->readings.FRMSVoltage;
    float voltageTHD = (sqrtf(VRMS2 - VFRMS2) / phase->readings.FRMSVoltage);
    return voltageTHD * 100.0f*Gain_THD;
}

/*!
 * @brief calculate current THD
 * @param[in] phase  The phase metrology
 * @return Current THD in percentage
 */
float calculateCurrentTHD(phaseMetrology *phase)
{
    float IRMS2 = phase->readings.RMSCurrent * phase->readings.RMSCurrent;
    float IFRMS2 = phase->readings.FRMSCurrent * phase->readings.FRMSCurrent;
    float currentTHD = (sqrtf(IRMS2 - IFRMS2) / phase->readings.FRMSCurrent);
    return currentTHD * 100.0f;
}

/*!
 * @brief calculate mains frequency
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return Mains Frequency (IQ15 format)
 */
float calculateMainsfrequency(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    uint32_t period;
    uint32_t quardOffset;

    period = phase->params.voltagePeriod.period;
    quardOffset = period >> 2;
    Metrology_setPhaseCorrection(&phase->params.current.quadratureCorrection, quardOffset - phaseCal->current.phaseOffset);

    phase->params.purePhaseRate = (4294967296.0f / (float)period)  * 256.0f ;

    float freq =  (((SAMPLES_PER_10_SECONDS / 10.0f) * 256.0f) / period)+0.50;//0.50 error correction(random)

    phase->params.interruptionLevel = ((MIN_SAG_VOLTAGE*MIN_SAG_VOLTAGE) / phaseCal->VscaleFactor) * (period / phaseCal->VscaleFactor);
    phase->params.interruptionLevelHalf = phase->params.interruptionLevel * (0.5f);

    return freq;
}

/*!
 * @brief    Calculate total active power
 * @param[in] workingData The metrology instance
 * @return Total Active Power in watts (IQ13 format)
 */
float calculateTotalActivePower(metrologyData *workingData)
{
    float activePower = 0.0f;

    int ph;
    for(ph = 0; ph < MAX_PHASES; ph++)
    {
        activePower += workingData->phases[ph].readings.activePower;
    }

    if(fabsf(activePower) < TOTAL_RESIDUAL_POWER_CUTOFF)
    {
        activePower = 0.0f;
    }
    return activePower;
}

/*!
 * @brief    Calculate total reactive power
 * @param[in] workingData The metrology instance
 * @return Total Reactive Power in VAR (IQ13 format)
 */
float calculateTotalReactivePower(metrologyData *workingData)
{
    float reactivePower = 0.0f;
    int ph;
    for(ph = 0; ph < MAX_PHASES; ph++)
    {
        reactivePower += workingData->phases[ph].readings.reactivePower;
    }

    if(fabsf(reactivePower) < TOTAL_RESIDUAL_POWER_CUTOFF)
    {
        reactivePower = 0.0f;
    }
    return reactivePower;
}

/*!
 * @brief    Calculate total apparent power
 * @param[in] workingData The metrology instance
 * @return Total Apparent Power in VA (IQ13 format)
 */
float calculateTotalApparentPower(metrologyData *workingData)
{
    float apparentPower = 0.0f;
    int ph;
    for(ph = 0; ph < MAX_PHASES; ph++)
    {
        apparentPower += workingData->phases[ph].readings.apparentPower;
    }

    if(fabsf(apparentPower) < TOTAL_RESIDUAL_POWER_CUTOFF)
    {
        apparentPower = 0.0f;
    }
    return apparentPower;
}

/*!
 * @brief    Calculate total fundamental active power
 * @param[in] workingData The metrology instance
 * @return Total Fundamental Active Power in watts (IQ13 format)
 */
float calculateTotalFundamentalActivePower(metrologyData *workingData)
{
    float fundamentalActivePower = 0.0f;
    int ph;
    for(ph = 0; ph < MAX_PHASES; ph++)
    {
        fundamentalActivePower += workingData->phases[ph].readings.FActivePower;
    }

    if(fabsf(fundamentalActivePower) < TOTAL_RESIDUAL_POWER_CUTOFF)
    {
        fundamentalActivePower = 0.0f;
    }
    return fundamentalActivePower;
}

/*!
 * @brief    Calculate total fundamental reactive power
 * @param[in] workingData The metrology instance
 * @return Total Fundamental Reactive Power in VAR (IQ13 format)
 */
float calculateTotalFundamentalReactivePower(metrologyData *workingData)
{
    float fundamentalReactivePower = 0.0f;
    int ph;
    for(ph = 0; ph < MAX_PHASES; ph++)
    {
        fundamentalReactivePower += workingData->phases[ph].readings.FReactivePower;
    }

    if(fabsf(fundamentalReactivePower) < TOTAL_RESIDUAL_POWER_CUTOFF)
    {
        fundamentalReactivePower = 0.0f;
    }
    return fundamentalReactivePower;
}

/*!
 * @brief    Calculate total fundamental apparent power
 * @param[in] workingData The metrology instance
 * @return Total Fundamental Apparent Power in VA (IQ13 format)
 */
float calculateTotalFundamentalApparentPower(metrologyData *workingData)
{
    float fundamentalApparentPower = 0.0f;
    int ph;
    for(ph = 0; ph < MAX_PHASES; ph++)
    {
        fundamentalApparentPower += workingData->phases[ph].readings.FApparentPower;
    }

    if(fabsf(fundamentalApparentPower) < TOTAL_RESIDUAL_POWER_CUTOFF)
    {
        fundamentalApparentPower = 0.0f;
    }
    return fundamentalApparentPower;
}

/*!
 * @brief    Calculate line to line voltage
 * @param[in] phase1  The phase1 metrology
 * @param[in] phase2  The phase2 metrology
 * @return    line to line voltage in volts
 */
float calculateLinetoLineVoltage(phaseMetrology *phase1, phaseMetrology *phase2)
{
    float llVoltage;

    float VRMS1 = phase1->readings.RMSVoltage;
    float VRMS2 = phase2->readings.RMSVoltage;
    float angle = (phase1->readings.phasetoPhaseAngle) * (0.5f);

    float x, y;

    x = VRMS1 - (VRMS2 * cosf(angle*6.283185307f));
    y = VRMS2 * sinf(angle*6.283185307f);

    llVoltage = hypotf(x, y);

    return llVoltage;
}

/*!
 * @brief    Calculate fundamental line to line voltage
 * @param[in] phase1  The phase1 metrology
 * @param[in] phase2  The phase2 metrology
 * @return    fundamental line to line voltage in volts
 */
float calculateFundamentalLinetoLineVoltage(phaseMetrology *phase1, phaseMetrology *phase2)
{
    float llFVoltage;

    float VFRMS1 = phase1->readings.FRMSVoltage;
    float VFRMS2 = phase2->readings.FRMSVoltage;
    float angle = (phase1->readings.phasetoPhaseAngle) * (0.5f);


    float x, y;

    x = VFRMS1 - (VFRMS2 * cosf(angle*6.283185307f));
    y = (VFRMS2 * sinf(angle*6.283185307f));

    llFVoltage = hypotf(x,y);
    return llFVoltage;
}

/*!
 * @brief    Calculate aggregate power factor
 * @param[in] workingData The metrology instance
 * @return Aggregate power factor in IQ13 format
 */
float calculateAggregatePowerfactor(metrologyData *workingData)
{
    totalParams total = workingData->totals;

    float powerFactor = (total.readings.activePower / total.readings.apparentPower);

    if(total.readings.reactivePower > 0.0f)
    {
        powerFactor = -powerFactor;
    }
    if(total.readings.activePower < 0.0f)
    {
        powerFactor = -powerFactor;
    }

  return powerFactor;
}

/*!
 * @brief    Calculate current vector sum
 * @param[in] workingData The metrology instance
 * @return Current vector sum in amps
 */
float calculateVectorCurrentSum(metrologyData *workingData)
{
    float vectorCurrentSum = 0;

#ifdef THREE_PHASE_SUPPORT
    float current0 = workingData->phases[0].readings.RMSCurrent;
    float current1 = workingData->phases[1].readings.RMSCurrent;
    float current2 = workingData->phases[2].readings.RMSCurrent;
    float angle1 = ((workingData->phases[0].readings.phasetoPhaseAngle + workingData->phases[0].readings.powerFactorAngle - workingData->phases[1].readings.powerFactorAngle) * (0.5f));
    float angle2 = ((workingData->phases[1].readings.phasetoPhaseAngle + workingData->phases[1].readings.powerFactorAngle - workingData->phases[2].readings.powerFactorAngle) * (0.5f));
    float ix, iy;


    ix = current0 + (current1 * cosf(angle1*6.283185307f)) + (current2 * cosf((angle2 + angle1)*6.283185307f)); //for cosPU

    iy = (current1 * sinf(angle1*6.283185307f)) + (current2 * sinf((angle2 + angle1)*6.283185307f)); //for sinPU

    vectorCurrentSum = hypotf(ix,iy);
#endif
    return vectorCurrentSum;
}

/*!
 * @brief calculate Neutral RMS Current
 * @param[in] neutral    The neutral metrology
 * @param[in] neutralCal The neutral calibration data
 * @return Neutral RMS current in amps
 */
float calculateNeutralRMSCurrent(neutralMetrology *neutral, currentSensorCalibrationData *neutralCal)
{
    float NeutralRMSCurrent;

    /* index of the dot product currently set for foreground processing */
    int8_t dp = neutral->params.dpSet;
    neutralDPSet *current = &neutral->params.dotProd[dp];

    /* RMS Neutral Current calculation  */
    if(!neutral->params.dotProd[dp].sampleCount)
    {
        /* Number of samples is zero    */
        NeutralRMSCurrent = 0.0f;
    }
    else
    {
        float sqrms = (float)current->currentSq/(float)current->sampleCount;
        float rms = sqrtf(sqrms);

        NeutralRMSCurrent = (rms * (float)(neutralCal->IscaleFactor));

        if(NeutralRMSCurrent < neutralCal->IacOffset)
        {
            /* RMS current is smaller than offset error */
            NeutralRMSCurrent = 0.0f;
        }
        else
        {
            NeutralRMSCurrent -= neutralCal->IacOffset;
        }
    }
    return NeutralRMSCurrent;
}

/*!
 * @brief    Read Line to Line Voltage
 * param[in] workingData The metrology instance
 */
void readLinetoLineVoltage(metrologyData *workingData)
{
#ifdef THREE_PHASE_SUPPORT
    workingData->LLReadings.LLRMSVoltage_ab = calculateLinetoLineVoltage(&workingData->phases[PHASE_ONE], &workingData->phases[PHASE_TWO]);
    workingData->LLReadings.LLRMSVoltage_bc = calculateLinetoLineVoltage(&workingData->phases[PHASE_TWO], &workingData->phases[PHASE_THERE]);
    workingData->LLReadings.LLRMSVoltage_ca = calculateLinetoLineVoltage(&workingData->phases[PHASE_THERE], &workingData->phases[PHASE_ONE]);
#endif
}

/*!
 * @brief    Read Fundamental Line to Line Voltage
 * param[in] workingData The metrology instance
 */
void readFundamentalLinetoLineVoltage(metrologyData *workingData)
{
#ifdef THREE_PHASE_SUPPORT
    workingData->LLReadings.FLLRMSVoltage_ab = calculateFundamentalLinetoLineVoltage(&workingData->phases[PHASE_ONE], &workingData->phases[PHASE_TWO]);
    workingData->LLReadings.FLLRMSVoltage_bc = calculateFundamentalLinetoLineVoltage(&workingData->phases[PHASE_TWO], &workingData->phases[PHASE_THERE]);
    workingData->LLReadings.FLLRMSVoltage_ca = calculateFundamentalLinetoLineVoltage(&workingData->phases[PHASE_THERE], &workingData->phases[PHASE_ONE]);
#endif
}

/*!
 * @brief Integrate Power to Energy
 * @param[in] en          The energy instance
 * @param[in] power       Power
 * @param[in] samples     Number of samples
 * @return energy steps
 */
float integratePowertoEnergy(energyIntegrator *en, float power, uint32_t samples)
{
    volatile float energy;
    volatile float energyStep;

    energy = (((power) * ((float)samples / SAMPLE_RATE))) + en->energyResidual;
    energyStep = 0.0f;
    while(energy >= ENERGY_100mWATT_HOUR_THRESHOLD_DBL)
    {
        energy -= ENERGY_100mWATT_HOUR_THRESHOLD_DBL;
        energyStep += 1.0f;
    }

    en->energyResidual = energy;

    return energyStep;
}

/*!
 * @brief Cycle RMS Voltage Calculation
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 * @return cycle RMS Voltage in volts
 */
float calculateCycleRMSVoltage(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];
    phaseCalibrationData *phaseCal = &calInfo->phases[ph];

    float voltage;

    int cycleDp = phase->params.cycleDpSet;
    cyclePhaseDPSet *dpSet = &phase->params.cycleDotProd[cycleDp];

    if(!dpSet->cycleSampleCount)
    {
        voltage = 0.0f;
    }
    else
    {
        float sqrms = (float)dpSet->cycleVoltageSq/dpSet->cycleSampleCount;
        float rms = sqrtf(sqrms);
        voltage = (rms * (phaseCal->VscaleFactor));

        if(voltage < phaseCal->VacOffset)
        {
            voltage = 0.0f;
        }
        else
        {
            voltage -= phaseCal->VacOffset;
        }
    }

    phase->readings.RMSVoltageCycle = voltage;
    return voltage;
}

/*!
 * @brief Check for sag swell events
 * @param[in] workingData   The Metrology Instance
 * @param[in] ph            Phase number
 */
void checkSagSwellEvents(metrologyData *workingData, PHASES ph)
{
    phaseMetrology *phase = &workingData->phases[ph];

    float cycleRMSVoltage = calculateCycleRMSVoltage(workingData, ph);

    if(cycleRMSVoltage < phase->params.sagThresholdStart)
    {
        /* sag condition    */
        if(cycleRMSVoltage > phase->params.sagMinVoltage)
        {
            if((phase->params.sagSwellState != SAG_SWELL_VOLTAGE_SAG_ONSET)
                    && (phase->params.sagSwellState != SAG_SWELL_VOLTAGE_SAG_CONTINUING))
            {
                /* new sag condition    */
                phase->params.sagSwellState = SAG_SWELL_VOLTAGE_SAG_ONSET;
                phase->params.sagValue = cycleRMSVoltage;

                phase->params.sagEvents ++;
                phase->params.sagDuration = 1;
            }
            else
            {
                /* continuing sag condition */
                phase->params.sagSwellState = SAG_SWELL_VOLTAGE_SAG_CONTINUING;
                if(cycleRMSVoltage < phase->params.sagValue)
                {
                    phase->params.sagValue = cycleRMSVoltage;
                }
                phase->params.sagDuration++;
            }
        }
        else
        {
            if((phase->params.sagSwellState != SAG_SWELL_VOLTAGE_INTERRUPTION_ONSET)
                    &&(phase->params.sagSwellState != SAG_SWELL_VOLTAGE_INTERRUPTION_CONTINUING))
            {
                /* new interruption */
                phase->params.sagSwellState = SAG_SWELL_VOLTAGE_INTERRUPTION_ONSET;
                phase->params.interruptionEvents++;
                phase->params.interruptionDuration = 1;
            }
            else
            {
                /* continue interruption    */
                phase->params.sagSwellState = SAG_SWELL_VOLTAGE_INTERRUPTION_CONTINUING;
                phase->params.interruptionDuration++;
            }

        }
    }
    else if(cycleRMSVoltage > phase->params.swellThresholdStart)
    {
        /* swell condition  */
        if((phase->params.sagSwellState != SAG_SWELL_VOLTAGE_SWELL_ONSET)
                && (phase->params.sagSwellState != SAG_SWELL_VOLTAGE_SWELL_CONTINUING))
        {
            /* new swell condition  */
            phase->params.sagSwellState = SAG_SWELL_VOLTAGE_SWELL_ONSET;
            phase->params.swellValue = cycleRMSVoltage;
            phase->params.swellEvents++;
            phase->params.swellDuration = 1;
        }
        else
        {
            /* continuing swell condition   */
            phase->params.sagSwellState = SAG_SWELL_VOLTAGE_SWELL_CONTINUING;
            if(cycleRMSVoltage > phase->params.swellValue)
            {
                phase->params.swellValue = cycleRMSVoltage;
            }
            phase->params.swellDuration++;
        }
    }
    else
    {
        if((cycleRMSVoltage > phase->params.sagThresholdStop)
                &&((phase->params.sagSwellState == SAG_SWELL_VOLTAGE_SAG_CONTINUING)
                        || (phase->params.sagSwellState == SAG_SWELL_VOLTAGE_SAG_ONSET)
                        || (phase->params.sagSwellState == SAG_SWELL_VOLTAGE_INTERRUPTION_CONTINUING)
                        || (phase->params.sagSwellState == SAG_SWELL_VOLTAGE_INTERRUPTION_ONSET)))
        {
            /* sag ended    */
            phase->params.sagSwellState = SAG_SWELL_VOLTAGE_NORMAL;
        }
        else if((cycleRMSVoltage < phase->params.swellThresholdStop)
                &&((phase->params.sagSwellState == SAG_SWELL_VOLTAGE_SWELL_CONTINUING)
                        || (phase->params.sagSwellState == SAG_SWELL_VOLTAGE_SWELL_ONSET)))
        {
            /* swell ended  */
            phase->params.sagSwellState = SAG_SWELL_VOLTAGE_NORMAL;
        }
    }
}

/*!
 * @brief calculate dc estimate voltage
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return  DC Estimate Voltage in volts (float precision)
 */
float calculateVdcEstimate(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float Vdclogged = (float)phase->params.V_dc_estimate;

    // Apply calibration scaling factor
    float VdcEstimate = Vdclogged * phaseCal->VscaleFactor;

    return VdcEstimate;
}

/*!
 * @brief calculate dc estimate current
 * @param[in] phase    The phase metrology
 * @param[in] phaseCal The phase calibration data
 * @return  DC Estimate current in amps (float precision)
 */
float calculateIdcEstimate(phaseMetrology *phase, phaseCalibrationData *phaseCal)
{
    float Idclogged = (float)phase->params.current.I_dc_estimate;

    // Apply calibration scaling factor
    float IdcEstimate = Idclogged * phaseCal->current.IscaleFactor;

    return IdcEstimate;
}

/*!
 * @brief calculate neutral dc estimate current
 * @param[in] neutral    The neutral metrology
 * @param[in] neutralCal The neutral calibration data
 * @return  Neutral DC Estimate current in amps (float precision)
 */
float calculateNeutralIdcEstimate(neutralMetrology *neutral, currentSensorCalibrationData *neutralCal)
{
    float Idclogged = (float)neutral->params.I_dc_estimate;

    // Apply calibration scaling factor
    float IdcEstimate = Idclogged * neutralCal->IscaleFactor;

    return IdcEstimate;
}

#ifdef HARMONICS_SUPPORT
/*******************************************************************************************************************//**
 *
 * @brief evaluateVthdFFT()
 *          Calculates Voltage Harmonic THD for h >= 2 using FFT results.
 *          THD = sqrtf( Sum(Yh^2) / Y1^2 ) for h = 2 to MAX_HARMONIC
 *   where:
 *    Yh^2      = Power (RMS squared) of the h-th harmonic
 *    Y1        = Magnitude (RMS) of the fundamental
 *
 * @param[in] phase         pointer to structure holding all the working data for one phase
 *
 * @return Voltage THD (as a ratio, multiply by 100 for percentage)
 */
float evaluateVthdFFT(phaseMetrology *phase)
{
    int16_t i;
    float p_harmonic[MAX_HARMONIC];
    float thd = 0.0f;
    float sum_harmonics = 0.0f;
    float fundamental_power;
    uint32_t index;

    if(phase->params.totalSampleCount > FFT_SIZE) {
        index = phase->params.vInputBufferIndex;
    }
    else {
        index = 0;
    }
    find_harmonics( phase->params.vInputBuffer, index, p_harmonic);

    for(i=1; i<MAX_HARMONIC; i++) {
        sum_harmonics += p_harmonic[i];
    }

    // Calculate fundamental power = fundamental magnitude squared
    fundamental_power = p_harmonic[0];// * p_harmonic[0];

    // Avoid division by zero if fundamental power is zero or very small
    if (fundamental_power <= 1e-12f) { // Use a small threshold instead of exact zero for floating point
        return 0.0f; // Or handle as an error/undefined case
    }

    thd = sqrtf(sum_harmonics / fundamental_power);

    return thd * 100.0f;
}

/*******************************************************************************************************************//**
 *
 * @brief evaluateIthdFFT()
 *          Calculates Current Harmonic THD for h >= 2
 *          Ythd,h = sqrtf( Y(g,h)^2 / Y(g,1)^2 )
 *   where:
 *    Y(g,h)    = RMS power of grouped of hth harmonic
 *    Y(g,1)    = RMS power of fundamental
 *
 * @param[in] phase         pointer to structure holding all the working data for one phase
 * @param[out] p_harmonics  pointer to Current Harmonic THD structure
 *
 * @return none
 */
float evaluateIthdFFT(phaseMetrology *phase)
{
    int16_t i;
    float p_harmonic[MAX_HARMONIC];
    float thd = 0.0f;
    float sum_harmonics = 0.0f;
    float fundamental_power;
    uint32_t index;

    if(phase->params.current.totalSampleCount > FFT_SIZE) {
        index = phase->params.current.iInputBufferIndex;
    }
    else {
        index = 0;
    }

    find_harmonics( phase->params.current.iInputBuffer, index, p_harmonic);

    for(i=1; i<MAX_HARMONIC; i++) {
        sum_harmonics += p_harmonic[i];
    }

    // Calculate fundamental power = fundamental magnitude squared
    fundamental_power = p_harmonic[0];// * p_harmonic[0];

    // Avoid division by zero if fundamental power is zero or very small
    if (fundamental_power <= 1e-12f) { // Use a small threshold instead of exact zero for floating point
        return 0.0f; // Or handle as an error/undefined case
    }

    thd = sqrtf(sum_harmonics / fundamental_power);

    return thd * 100.0f;
}
#endif
