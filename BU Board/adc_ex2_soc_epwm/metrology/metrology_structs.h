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
 *  @brief      Structures for RMS Current measurement
 *
 *  @anchor metrology_structs_h
 *  # Overview
 *
 *  Simplified structures for RMS Current measurement only.
 *
 *  <hr>
 ******************************************************************************/
/** @addtogroup Metrology
 * @{
 */

#ifndef METROLOGY_STRUCTS_H_
#define METROLOGY_STRUCTS_H_

#include "inc/hw_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#include "metrology_nv_structs.h"

/*! @brief Define Phase Readings (RMS Current only) */
typedef struct {
    float RMSCurrent;
} phaseReadings;

/*! @brief Defines current dot product set (simplified for RMS Current) */
typedef struct {
    float currentSq;
    uint32_t sampleCount;
} currentDPSet;

/*! @brief Current parameters (simplified for RMS Current) */
typedef struct {
    float I_dc_estimate;
    float I_dc_estimate_logged;
    currentDPSet dotProd[2];
    float IHistory[2];
    int currentEndStops;
    uint8_t dpSet;
} currentParams;

/*! @brief Defines Phase parameters (simplified for RMS Current) */
typedef struct {
    currentParams current;
    uint8_t dpSet;
    volatile uint16_t status;
} phaseParams;

/*! @brief Defines Phase metrology (simplified for RMS Current) */
typedef struct {
    phaseReadings readings;
    phaseParams params;
    volatile uint16_t status;
} phaseMetrology;

/*! @brief Defines Metrology Data (simplified for RMS Current) */
typedef struct {
    phaseMetrology phases[MAX_PHASES];
    float rawCurrentData[MAX_PHASES];
    float lastRawCurrentData[MAX_PHASES];
    METROLOGY_STATUS metrologyState;
    OPERATING_MODES operatingMode;
} metrologyData;

#ifdef __cplusplus
}
#endif
#endif /* METROLOGY_STRUCTS_H_ */
/** @}*/
