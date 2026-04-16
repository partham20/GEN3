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
 *  @file       metrology_structs.h
 *  @brief      Structures in metrology module
 *
 *  @anchor metrology_structs_h
 *  # Overview
 *
 *  Structures in metrology module
 *
 *  <hr>
 ******************************************************************************/
/** @addtogroup Metrology
 * @{
 */

/*!
 * @file metrology_structs.h
 * @brief Core data structures for the metrology system
 *
 * This file defines the data structures used throughout the metrology system
 * to store measurement data, calculation parameters, and system state. These
 * structures organize the data for voltage, current, power, and energy measurements
 * across multiple phases.
 *
 * The structures support various power metering configurations including
 * single-phase, split-phase, and three-phase systems with optional neutral
 * monitoring for tamper detection.
 */

#ifndef METROLOGY_STRUCTS_H_
#define METROLOGY_STRUCTS_H_

#include "inc/hw_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "metrology_nv_structs.h"

/*! @brief Define Phase Readings */
typedef struct {
    float RMSVoltage;          
    float RMSVoltageCycle;
    float RMSCurrent;
    float activePower;
    float reactivePower;
    float apparentPower;
    float FRMSVoltage;
    float FRMSCurrent;
    float FActivePower;
    float FReactivePower;
    float FApparentPower;
    float voltageTHD;
    float currentTHD;
    float powerFactor;
    float powerFactorAngle;
    float underdeviation;
    float overdeviation;
    float frequency;
    float phasetoPhaseAngle;
    float vTHDfft;
    float iTHDfft;
} phaseReadings;

/*! @brief Defines neutral readings */
typedef struct {
    float RMSCurrent;          
} neutralReadings;

/*! @brief Defines Total Power readings */
typedef struct {
    float activePower;
    float reactivePower;
    float apparentPower;
    float fundamentalActivePower;
    float fundamentalReactivePower;
    float fundamentalApparentPower;
} TotalPowerReadings;

/*! @brief Defines energy integrator parameters */
typedef struct {
    float energy;              
    float energyResidual;
} energyIntegrator;

/*! @brief Defines Line Parameters  */
typedef struct
{
    float LLRMSVoltage_ab;
    float FLLRMSVoltage_ab;
    float LLRMSVoltage_bc;
    float FLLRMSVoltage_bc;
    float LLRMSVoltage_ca;
    float FLLRMSVoltage_ca;
}LLReadings;

/*! @brief Defines Phase Correction Parameters */
typedef struct {
    int8_t step;
    float firBeta;
    float firGain;            
} phaseCorrection;

/*! @brief Defines current dot product set */
typedef struct {
    float activePower;        
    float reactivePower;
    float FActivePower;
    float FReactivePower;
    float currentSq;
    uint32_t sampleCount;
} currentDPSet;

/*! @brief Defines voltage dot product set */
typedef struct {
    float voltageSq;
    float fVoltageSq;
    float fVoltageQuardSq;
    uint32_t sampleCount;
    int16_t cycleNumber;
} voltageDPSet;

/*! @brief Defines cycle voltage product set    */
typedef struct
{
    /*! @brief Cycle voltage product  */
    float cycleVoltageSq;
    /*! @brief Sample count  */
    uint32_t cycleSampleCount;
}cyclePhaseDPSet;

/*! @brief Defines neutral dot product set  */
typedef struct
{
    /*! @brief neutral current square   */
    float currentSq;
    /*! @brief neutral current sample count */
    uint32_t sampleCount;
}neutralDPSet;

/*! @brief current parameters */
typedef struct {
    float I_dc_estimate;
//    float I_dc_estimate_integral;
    float I_dc_estimate_logged;
    currentDPSet dotProd[2];
    phaseCorrection phaseCorrection;
    phaseCorrection quadratureCorrection;
    float IHistory[2];
//    int currentBufferIndex;
    int currentEndStops;
//    float iInputBuffer[FFT_SIZE];
//    uint16_t iInputBufferIndex;
//    uint32_t totalSampleCount;
} currentParams;

/*! @brief Defines mains parameters */
typedef struct {
    uint32_t cycleSamples;       
    uint32_t period;
    float cyclePeriod;       
} mainsParams;


//phases

/*! @brief Defines Phase parameters */
typedef struct {
    float V_dc_estimate;
////    float V_dc_estimate1;
      float V_dc_estimate_logged;
    float vHistory[64];
    int8_t vHistoryIndex;
////    // float purePhase;
    uint32_t purePhase;
    float purePhaseRate;
    float crossSum;
    int sinceLast;
    float lastVoltageSample;
    int voltageEndStops;
    voltageDPSet dotProd[2];
    cyclePhaseDPSet cycleDotProd[2];
    currentParams current;
    mainsParams voltagePeriod;
    uint8_t dpSet;
    uint8_t cycleDpSet;
    float sagThresholdStart;
    float sagMinVoltage;
    float sagThresholdStop;
    float sagValue;
    float swellThresholdStart;
    float swellThresholdStop;
    float swellValue;
    int8_t sagSwellState;
    float interruptionLevel;
    float interruptionLevelHalf;
    uint16_t interruptionEvents;
    uint32_t interruptionDuration;
    uint16_t sagEvents;
    uint32_t sagDuration;
    uint16_t swellEvents;
    uint32_t swellDuration;
    uint8_t cycleSkipCount;
    float vInputBuffer[FFT_SIZE];
    uint16_t vInputBufferIndex;
    uint32_t totalSampleCount;
} phaseParams;

/*! @brief Defines neutral Params */
typedef struct {
    float I_dc_estimate;     
    float I_dc_estimate_logged;
    neutralDPSet dotProd[2];
    float I_History[2];     
    phaseCorrection phaseCorrection;
    int8_t currentEndStops;
    uint8_t dpSet;
    float iInputBuffer[FFT_SIZE];
    uint16_t iInputBufferIndex;
} neutralParams;

/*! @brief Defines energy pulse params */
typedef struct {
    float integrator;    
    uint8_t pulseRemainingTime;
} energyPulse;

/*! @brief Defines Energy Directions    */
typedef struct
{
    /*! @brief Struct to store active energy pulse  */
    energyPulse activeEnergyPulse;
    /*! @brief Struct to store reactive energy pulse  */
    energyPulse reactiveEnergyPulse;
    /*! @brief Struct to store apparent energy pulse  */
    energyPulse apparentEnergyPulse;
    /*! @brief Struct to store fundamental active energy pulse  */
    energyPulse fActiveEnergyPulse;
    /*! @brief Struct to store fundamental reactive energy pulse  */
    energyPulse fReactiveEnergyPulse;
    /*! @brief Struct to store fundamental apparent energy pulse  */
    energyPulse fApparentEnergyPulse;
    /*! @brief Active Energy imported   */
    energyIntegrator activeEnergyImported;
    /*! @brief Active Energy exported   */
    energyIntegrator activeEnergyExported;
    /*! @brief Reactive Energy Quadrant I   */
    energyIntegrator reactiveEnergyQuardrantI;
    /*! @brief Reactive Energy Quadrant II   */
    energyIntegrator reactiveEnergyQuardrantII;
    /*! @brief Reactive Energy Quadrant III  */
    energyIntegrator reactiveEnergyQuardrantIII;
    /*! @brief Reactive Energy Quadrant IV   */
    energyIntegrator reactiveEnergyQuardrantIV;
    /*! @brief Apparent Energy imported   */
    energyIntegrator appparentEnergyImported;
    /*! @brief Apparent Energy exported   */
    energyIntegrator appparentEnergyExported;
    /*! @brief Fundamental Active Energy imported   */
    energyIntegrator fundamentalActiveEnergyImported;
    /*! @brief Fundamental Active Energy exported   */
    energyIntegrator fundamentalActiveEnergyExported;
    /*! @brief Fundamental Reactive Energy Quadrant I   */
    energyIntegrator fundamentalReactiveEnergyQuardrantI;
    /*! @brief Fundamental Reactive Energy Quadrant II   */
    energyIntegrator fundamentalReactiveEnergyQuardrantII;
    /*! @brief Fundamental Reactive Energy Quadrant III  */
    energyIntegrator fundamentalReactiveEnergyQuardrantIII;
    /*! @brief Fundamental Reactive Energy Quadrant IV   */
    energyIntegrator fundamentalReactiveEnergyQuardrantIV;
    /*! @brief Fundamental Apparent Energy imported   */
    energyIntegrator fundamentalAppparentEnergyImported;
    /*! @brief Fundamental Apparent Energy exported   */
    energyIntegrator fundamentalAppparentEnergyExported;
}phaseEnergy;

/*! @brief Defines Phase metrology  */
typedef struct
{
    /*! @brief Phase Readings   */
    phaseReadings  readings;
    /*! @brief Phase Params */
    phaseParams    params;
    /*! @brief Phase Energies */
    phaseEnergy    energy;
//    /*! @brief Phase Status */
    volatile uint16_t       status;
//    /*! @brief Phase cCycle Status */
    volatile uint16_t       cycleStatus;
}phaseMetrology;

/*! @brief Defines Neutral Metrology    */
typedef struct
{
    /*! @brief Neutral Readings   */
    neutralReadings  readings;
    /*! @brief Neutral Params   */
    neutralParams    params;
    /*! @brief Neutral Status   */
    volatile uint16_t       status;
}neutralMetrology;

/*! @brief Defines Total Parameters */
typedef struct {
    TotalPowerReadings readings;
    phaseEnergy energy;
    float powerFactor;   
    float currentVectorSum; 
} totalParams;

/*! @brief Defines Metrology Data */
typedef struct {
    phaseMetrology phases[MAX_PHASES];
    neutralMetrology neutral;
    totalParams totals;
    LLReadings LLReadings;
    float rawVoltageData[MAX_PHASES];
    float rawCurrentData[MAX_PHASES];
    float lastRawCurrentData[MAX_PHASES];
    float currentIntegrationData[MAX_PHASES];
    float rawNeutralData;
    METROLOGY_STATUS metrologyState;
    OPERATING_MODES operatingMode;
    bool activePulse;
    bool reactivePulse;
} metrologyData;

#ifdef __cplusplus
}
#endif
#endif /* METROLOGY_STRUCTS_H_ */
/** @}*/
