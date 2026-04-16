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
 *  @file       metrology_calibration_defaults.h
 *  @brief      Default calibration parameters
 *
 *  @anchor metrology_calibration_defaults_h
 *  # Overview
 *
 *
 *
 *  <hr>
 ******************************************************************************/
/** @addtogroup Metrology
 * @{
 */

/*!
 * @file metrology_calibration_defaults.h
 * @brief Default calibration parameters for metrology system
 * 
 * This file defines the default calibration parameters used when no calibration
 * data is available in non-volatile memory. These values represent typical
 * scaling factors, offsets, and correction values for a standard hardware
 * configuration. For production systems, these values should be replaced
 * with device-specific calibration data.
 */

#ifndef _METROLOGY_CALIBRATION_DEFAULTS_H_
#define _METROLOGY_CALIBRATION_DEFAULTS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "string.h"
#include "template.h"

/*! @brief Structure to store default calibration parameters */
extern calibrationData calibrationDefaults;

/*!
 * @brief Magic number to verify calibration data integrity in flash
 * 
 * This value is stored with calibration data to verify that the flash
 * contains valid calibration information.
 */
#define CONFIG_INIT_FLAG                               (0xABCD)

#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT) || defined(SINGLE_PHASE_SUPPORT)
/*!
 * @brief Default voltage scale factor for Phase A
 * 
 * Converts raw ADC readings to actual voltage values:
 * - Based on voltage divider ratio, ADC reference, and ADC resolution
 * - Defined in template.h for hardware-specific configuration
 */
#define DEFAULT_V_RMS_SCALE_FACTOR_A                       (float)(PHASE_A_VOLTAGE_SCALE_FACTOR)
//secondary
#define DEFAULT_V_RMS_SCALE_FACTOR_A_OUT                   (float)(PHASE_A_VOLTAGE_SCALE_FACTOR_OUT)
/*!
 * @brief Default initial DC offset estimate for voltage channel in Phase A
 * 
 * Used to improve initial settling time of voltage DC removal filter
 */
#define DEFAULT_V_DC_ESTIMATE_A                            (float)(PHASE_A_VOLTAGE_DC_ESTIMATE)
//secondary
#define DEFAULT_V_DC_ESTIMATE_A_OUT                        (float)(PHASE_A_VOLTAGE_DC_ESTIMATE_OUT)
/*! @brief Defines the voltage AC offset in phase A             */
#define DEFAULT_V_AC_OFFSET_A                           (float)(PHASE_A_VOLTAGE_AC_OFFSET)
//secondary
#define DEFAULT_V_AC_OFFSET_A_OUT                        (float)(PHASE_A_VOLTAGE_AC_OFFSET_OUT)
/*! @brief Defines the fundamental voltage AC offset in phase A */
#define DEFAULT_V_FUNDAMENTAL_OFFSET_A                      (float)(PHASE_A_VOLTAGE_FUNDAMENTAL_OFFSET)
//secondary
#define DEFAULT_V_FUNDAMENTAL_OFFSET_A_OUT                  (float)(PHASE_A_VOLTAGE_FUNDAMENTAL_OFFSET_OUT)
/*! @brief Defines the scale factor for current at phase A      */
#define DEFAULT_I_RMS_SCALE_FACTOR_A                    (float)(PHASE_A_CURRENT_SCALE_FACTOR)
/*! @brief Defines the scale factor for power at phase A        */
#define DEFAULT_P_SCALE_FACTOR_A                        (float)(PHASE_A_POWER_SCALE_FACTOR)
/*! @brief Defines the current DC estimate in phase A           */
#define DEFAULT_I_DC_ESTIMATE_A                         (float)(PHASE_A_CURRENT_DC_ESTIMATE)
/*! @brief Defines the current AC offset in phase A             */
#define DEFAULT_I_AC_OFFSET_A                           (float)(PHASE_A_CURRENT_AC_OFFSET)

//secondary
#define DEFAULT_I_DC_ESTIMATE_A_OUT                         (float)(PHASE_A_CURRENT_DC_ESTIMATE_OUT)
#define DEFAULT_I_AC_OFFSET_A_OUT                           (float)(PHASE_A_CURRENT_AC_OFFSET_OUT)
#define DEFAULT_I_FUNDAMENTAL_OFFSET_A_OUT                  (float)(PHASE_A_CURRENT_FUNDAMENTAL_OFFSET_OUT)
#define DEFAULT_I_RMS_SCALE_FACTOR_A_OUT                    (float)(PHASE_A_CURRENT_SCALE_FACTOR_OUT)

#define DEFAULT_P_OFFSET_A_OUT                               (float)(PHASE_A_ACTIVE_POWER_OFFSET_OUT)
#define DEFAULT_Q_OFFSET_A_OUT                               (float)(PHASE_A_REACTIVE_POWER_OFFSET_OUT)
#define DEFAULT_P_SCALE_FACTOR_A_OUT                         (float)(PHASE_A_POWER_SCALE_FACTOR_OUT)
#define DEFAULT_FUNDAMENTAL_P_OFFSET_A_OUT                   (float)(PHASE_A_ACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT)
#define DEFAULT_FUNDAMENTAL_Q_OFFSET_A_OUT                   (float)(PHASE_A_REACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT)


/*! @brief Defines the fundamental current AC offset in phase A */
#define DEFAULT_I_FUNDAMENTAL_OFFSET_A                  (float)(PHASE_A_CURRENT_FUNDAMENTAL_OFFSET)
/*! @brief Defines active power offset in phase A               */
#define DEFAULT_P_OFFSET_A                              (float)(PHASE_A_ACTIVE_POWER_OFFSET)
/*! @brief Defines fundamental active power offset in phase A   */
#define DEFAULT_FUNDAMENTAL_P_OFFSET_A                  (float)(PHASE_A_ACTIVE_POWER_FUNDAMENTAL_OFFSET)
/*! @brief Defines reactive power offset in phase A             */
#define DEFAULT_Q_OFFSET_A                              (float)(PHASE_A_REACTIVE_POWER_OFFSET)
/*! @brief Defines fundamental reactive power offset in phase A */
#define DEFAULT_FUNDAMENTAL_Q_OFFSET_A                  (float)(PHASE_A_REACTIVE_POWER_FUNDAMENTAL_OFFSET)
#endif 

#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT)
/*! @brief Defines the scale factor for voltage at phase B      */
#define DEFAULT_V_RMS_SCALE_FACTOR_B                    (float)(PHASE_B_VOLTAGE_SCALE_FACTOR)
//secondary
#define DEFAULT_V_RMS_SCALE_FACTOR_B_OUT                    (float)(PHASE_B_VOLTAGE_SCALE_FACTOR_OUT)
/*! @brief Defines the voltage DC estimate in phase B           */
#define DEFAULT_V_DC_ESTIMATE_B                             (float)(PHASE_B_VOLTAGE_DC_ESTIMATE)
//secondary
#define DEFAULT_V_DC_ESTIMATE_B_OUT                         (float)(PHASE_B_VOLTAGE_DC_ESTIMATE_OUT)
/*! @brief Defines the voltage AC offset in phase B             */
#define DEFAULT_V_AC_OFFSET_B                               (float)(PHASE_B_VOLTAGE_AC_OFFSET)
//secondary
#define DEFAULT_V_AC_OFFSET_B_OUT                           (float)(PHASE_B_VOLTAGE_AC_OFFSET_OUT)
/*! @brief Defines the fundamental voltage AC offset in phaseB  */
#define DEFAULT_V_FUNDAMENTAL_OFFSET_B                  (float)(PHASE_B_VOLTAGE_FUNDAMENTAL_OFFSET)
//secondary
#define DEFAULT_V_FUNDAMENTAL_OFFSET_B_OUT                  (float)(PHASE_B_VOLTAGE_FUNDAMENTAL_OFFSET_OUT)
/*! @brief Defines the scale factor for current at phase B      */
#define DEFAULT_I_RMS_SCALE_FACTOR_B                    (float)(PHASE_B_CURRENT_SCALE_FACTOR)
/*! @brief Defines the scale factor for power at phase B        */
#define DEFAULT_P_SCALE_FACTOR_B                        (float)(PHASE_B_POWER_SCALE_FACTOR)
/*! @brief Defines the current DC estimate in phase B           */
#define DEFAULT_I_DC_ESTIMATE_B                         (float)(PHASE_B_CURRENT_DC_ESTIMATE)

//secondary
#define DEFAULT_I_DC_ESTIMATE_B_OUT                         (float)(PHASE_B_CURRENT_DC_ESTIMATE_OUT)
#define DEFAULT_I_AC_OFFSET_B_OUT                           (float)(PHASE_B_CURRENT_AC_OFFSET_OUT)
#define DEFAULT_I_FUNDAMENTAL_OFFSET_B_OUT                  (float)(PHASE_B_CURRENT_FUNDAMENTAL_OFFSET_OUT)
#define DEFAULT_I_RMS_SCALE_FACTOR_B_OUT                    (float)(PHASE_B_CURRENT_SCALE_FACTOR_OUT)


#define DEFAULT_P_OFFSET_B_OUT                               (float)(PHASE_B_ACTIVE_POWER_OFFSET_OUT)
#define DEFAULT_Q_OFFSET_B_OUT                               (float)(PHASE_B_REACTIVE_POWER_OFFSET_OUT)
#define DEFAULT_P_SCALE_FACTOR_B_OUT                         (float)(PHASE_B_POWER_SCALE_FACTOR_OUT)
#define DEFAULT_FUNDAMENTAL_P_OFFSET_B_OUT                   (float)(PHASE_B_ACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT)
#define DEFAULT_FUNDAMENTAL_Q_OFFSET_B_OUT                   (float)(PHASE_B_REACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT)

/*! @brief Defines the current AC offset in phase B             */
#define DEFAULT_I_AC_OFFSET_B                           (float)(PHASE_B_CURRENT_AC_OFFSET)
/*! @brief Defines the fundamental current AC offset in phase B */
#define DEFAULT_I_FUNDAMENTAL_OFFSET_B                  (float)(PHASE_B_CURRENT_FUNDAMENTAL_OFFSET)
/*! @brief Defines active power offset in phase B               */
#define DEFAULT_P_OFFSET_B                              (float)(PHASE_B_ACTIVE_POWER_OFFSET)
/*! @brief Defines fundamental active power offset in phase B   */
#define DEFAULT_FUNDAMENTAL_P_OFFSET_B                  (float)(PHASE_B_ACTIVE_POWER_FUNDAMENTAL_OFFSET)
/*! @brief Defines reactive power offset in phase B             */
#define DEFAULT_Q_OFFSET_B                              (float)(PHASE_B_REACTIVE_POWER_OFFSET)
/*! @brief Defines fundamental reactive power offset in phase B */
#define DEFAULT_FUNDAMENTAL_Q_OFFSET_B                  (float)(PHASE_B_REACTIVE_POWER_FUNDAMENTAL_OFFSET)
#endif 

#if defined(THREE_PHASE_SUPPORT)
/*! @brief Defines the scale factor for voltage at phase C      */
#define DEFAULT_V_RMS_SCALE_FACTOR_C                    (float)(PHASE_C_VOLTAGE_SCALE_FACTOR)

//secondary
#define DEFAULT_V_RMS_SCALE_FACTOR_C_OUT                    (float)(PHASE_C_VOLTAGE_SCALE_FACTOR_OUT)
/*! @brief Defines the voltage DC estimate in phase C           */
#define DEFAULT_V_DC_ESTIMATE_C                         (float)(PHASE_C_VOLTAGE_DC_ESTIMATE)
//secondary
#define DEFAULT_V_DC_ESTIMATE_C_OUT                         (float)(PHASE_C_VOLTAGE_DC_ESTIMATE_OUT)
/*! @brief Defines the voltage AC offset in phase B             */
#define DEFAULT_V_AC_OFFSET_C                           (float)(PHASE_C_VOLTAGE_AC_OFFSET)
//secondary
#define DEFAULT_V_AC_OFFSET_C_OUT                           (float)(PHASE_C_VOLTAGE_AC_OFFSET_OUT)
/*! @brief Defines the fundamental voltage AC offset in phase B */
#define DEFAULT_V_FUNDAMENTAL_OFFSET_C                  (float)(PHASE_C_VOLTAGE_FUNDAMENTAL_OFFSET)
//secondary
#define DEFAULT_V_FUNDAMENTAL_OFFSET_C_OUT                  (float)(PHASE_C_VOLTAGE_FUNDAMENTAL_OFFSET)
/*! @brief Defines the scale factor for current at phase C      */
#define DEFAULT_I_RMS_SCALE_FACTOR_C                    (float)(PHASE_C_CURRENT_SCALE_FACTOR)
/*! @brief Defines the scale factor for power at phase C        */
#define DEFAULT_P_SCALE_FACTOR_C                        (float)(PHASE_C_POWER_SCALE_FACTOR)
/*! @brief Defines the current DC estimate in phase C           */
#define DEFAULT_I_DC_ESTIMATE_C                         (float)(PHASE_C_CURRENT_DC_ESTIMATE)
//secondary
#define DEFAULT_I_DC_ESTIMATE_C_OUT                         (float)(PHASE_C_CURRENT_DC_ESTIMATE_OUT)
#define DEFAULT_I_AC_OFFSET_C_OUT                           (float)(PHASE_C_CURRENT_AC_OFFSET_OUT)
#define DEFAULT_I_FUNDAMENTAL_OFFSET_C_OUT                  (float)(PHASE_C_CURRENT_FUNDAMENTAL_OFFSET_OUT)
#define DEFAULT_I_RMS_SCALE_FACTOR_C_OUT                    (float)(PHASE_C_CURRENT_SCALE_FACTOR_OUT)


#define DEFAULT_P_OFFSET_C_OUT                               (float)(PHASE_C_ACTIVE_POWER_OFFSET_OUT)
#define DEFAULT_Q_OFFSET_C_OUT                               (float)(PHASE_C_REACTIVE_POWER_OFFSET_OUT)
#define DEFAULT_P_SCALE_FACTOR_C_OUT                         (float)(PHASE_C_POWER_SCALE_FACTOR_OUT)
#define DEFAULT_FUNDAMENTAL_P_OFFSET_C_OUT                   (float)(PHASE_C_ACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT)
#define DEFAULT_FUNDAMENTAL_Q_OFFSET_C_OUT                   (float)(PHASE_C_REACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT)


/*! @brief Defines the current AC offset in phase C             */
#define DEFAULT_I_AC_OFFSET_C                           (float)(PHASE_C_CURRENT_AC_OFFSET)
/*! @brief Defines the fundamental current AC offset in phase C */
#define DEFAULT_I_FUNDAMENTAL_OFFSET_C                  (float)(PHASE_C_CURRENT_FUNDAMENTAL_OFFSET)
/*! @brief Defines active power offset in phase C               */
#define DEFAULT_P_OFFSET_C                              (float)(PHASE_C_ACTIVE_POWER_OFFSET)
/*! @brief Defines fundamental active power offset in phase C   */
#define DEFAULT_FUNDAMENTAL_P_OFFSET_C                  (float)(PHASE_C_ACTIVE_POWER_FUNDAMENTAL_OFFSET)
/*! @brief Defines reactive power offset in phase C             */
#define DEFAULT_Q_OFFSET_C                              (float)(PHASE_C_REACTIVE_POWER_OFFSET)
/*! @brief Defines fundamental reactive power offset in phase C */
#define DEFAULT_FUNDAMENTAL_Q_OFFSET_C                  (float)(PHASE_C_REACTIVE_POWER_FUNDAMENTAL_OFFSET)
#endif





#if defined(NEUTRAL_MONITOR_SUPPORT)
/*! @brief Defines the scale factor for current at neutral      */
#define DEFAULT_I_RMS_SCALE_FACTOR_NEUTRAL              (float)(NEUTRAL_CURRENT_SCALE_FACTOR)
/*! @brief Defines the current DC estimate in neutral           */
#define DEFAULT_I_DC_ESTIMATE_NEUTRAL                   (float)(NEUTRAL_CURRENT_DC_ESTIMATE)
/*! @brief Defines the current AC offset in neutral             */
#define DEFAULT_I_AC_OFFSET_NEUTRAL                     (float)(NEUTRAl_CURRENT_AC_OFFSET)
/*! @brief Defines the fundamental current AC offset in neutral */
#define DEFAULT_I_FUNDAMENTAL_OFFSET_NEUTRAL            (float)(NEUTRAL_CURRENT_FUNDAMENTAL_OFFSET)
/*! @brief Defines the scale factor for power at neutral        */
#define DEFAULT_P_SCALE_FACTOR_NEUTRAL                  (float)(NEUTRAL_POWER_SCALE_FACTOR)
/*! @brief Defines active power offset in neutral               */
#define DEFAULT_P_OFFSET_NEUTRAL                        (float)(NEUTRAL_ACTIVE_POWER_OFFSET)
/*! @brief Defines reactive power offset in neutral             */
#define DEFAULT_Q_OFFSET_NEUTRAL                        (float)(NEUTRAL_REACTIVE_POWER_OFFSET)
/*! @brief Defines fundamental active power offset in neutral   */
#define DEFAULT_FUNDAMENTAL_P_OFFSET_NEUTRAL            (float)(NEUTRAL_ACTIVE_POWER_FUNDAMENTAL_OFFSET)
/*! @brief Defines fundamental reactive power offset in neutral */
#define DEFAULT_FUNDAMENTAL_Q_OFFSET_NEUTRAL            (float)(NEUTRAL_REACTIVE_POWER_FUNDAMENTAL_OFFSET)
#endif

/* Value is phase angle in 1/256th of a sample increments. */

/** @def DEFAULT_BASE_PHASE_A_CORRECTION
    Defines holds the value for phase correction to compensate for delay due to the current
    transformer and/or front-end circuitry at phase A. This can be set to a value that is
    in fairly acceptable range, and it will be fine tuned under phase correction during calibration. */
/*  It is set to 0 (0f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS) when testing with simulated
 *  data as it doesn't required phase correction, any value between -125us to 125 us is acceptable and
 *  it is fine tuned during calibration, ideally it should be close to zero*/
#define DEFAULT_BASE_PHASE_A_CORRECTION             (0.0f)//(-2.4e-06f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS)
#define DEFAULT_BASE_PHASE_A_CORRECTION_OUT           (0.0f)//  (-2.4e-06f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS)



/** @def DEFAULT_BASE_PHASE_B_CORRECTION
    Defines holds the value for phase correction to compensate for delay due to the current
    transformer and/or front-end circuitry at phase B. This can be set to a value that is in
    fairly acceptable range, and it will be fine tuned under phase correction during calibration. */
/*  It is set to 0 (0f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS) when testing with simulated
 *  data as it doesn't required phase correction, any value between -125us to 125 us is acceptable and
 *  it is fine tuned during calibration, ideally it should be close to zero*/
#define DEFAULT_BASE_PHASE_B_CORRECTION             (0.0f)// (-3.9e-06f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS)
#define DEFAULT_BASE_PHASE_B_CORRECTION_OUT          (0.0f)//    (-2.4e-06f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS)



/** @def DEFAULT_BASE_PHASE_C_CORRECTION
    Defines holds the value for phase correction to compensate for delay due to the current
    transformer and/or front-end circuitry at phase C. This can be set to a value that is in
    fairly acceptable range, and it will be fine tuned under phase correction during calibration. */
/*  It is set to 0 (0f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS) when testing with simulated
 *  data as it doesn't required phase correction, any value between -125us to 125 us is acceptable and
 *  it is fine tuned during calibration, ideally it should be close to zero*/
#define DEFAULT_BASE_PHASE_C_CORRECTION           (0.0f)//   (1.5e-06f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS)
#define DEFAULT_BASE_PHASE_C_CORRECTION_OUT        (0.0f)//      (-2.4e-06f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS)


/** @def DEFAULT_BASE_NEUTRAL_PHASE_CORRECTION
    Defines holds the value for phase correction to compensate for delay due to the current
    transformer and/or front-end circuitry at neutral. This can be set to a value that is in
    fairly acceptable range, and it will be fine tuned under phase correction during calibration. */
/*  It is set to 0 (0f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS) when testing with simulated
 *  data as it doesn't required phase correction, any value between -125us to 125 us is acceptable and
 *  it is fine tuned during calibration, ideally it should be close to zero*/
#define DEFAULT_BASE_NEUTRAL_PHASE_CORRECTION       (1.5e-06f * SAMPLE_RATE * PHASE_SHIFT_FIR_TABLE_ELEMENTS)


/*! @brief Default calibration data to store in flash   */
calibrationData calibrationDefaults = {
    {
#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT) || defined(SINGLE_PHASE_SUPPORT)
        {
            {
                .IinitialDcEstimate = DEFAULT_I_DC_ESTIMATE_A,
                .IacOffset = DEFAULT_I_AC_OFFSET_A,
                .IFAcOffset = DEFAULT_I_FUNDAMENTAL_OFFSET_A,
                .activePowerOffset = DEFAULT_P_OFFSET_A,
                .reactivePowerOffset = DEFAULT_Q_OFFSET_A,
                .FActivePowerOffset = DEFAULT_FUNDAMENTAL_P_OFFSET_A,
                .FReactivePowerOffset = DEFAULT_FUNDAMENTAL_Q_OFFSET_A,
                .phaseOffset = DEFAULT_BASE_PHASE_A_CORRECTION,
                .IscaleFactor = DEFAULT_I_RMS_SCALE_FACTOR_A,
                .PscaleFactor = DEFAULT_P_SCALE_FACTOR_A,
            },
            .VinitialDcEstimate = DEFAULT_V_DC_ESTIMATE_A,
            .VacOffset = DEFAULT_V_AC_OFFSET_A,
            .VFundamentalAcOffset = DEFAULT_V_FUNDAMENTAL_OFFSET_A,
            .VscaleFactor = DEFAULT_V_RMS_SCALE_FACTOR_A,
        },
#endif

#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT)
        {
            {
                .IinitialDcEstimate = DEFAULT_I_DC_ESTIMATE_B,
                .IacOffset = DEFAULT_I_AC_OFFSET_B,
                .IFAcOffset = DEFAULT_I_FUNDAMENTAL_OFFSET_B,
                .activePowerOffset = DEFAULT_P_OFFSET_B,
                .reactivePowerOffset = DEFAULT_Q_OFFSET_B,
                .FActivePowerOffset = DEFAULT_FUNDAMENTAL_P_OFFSET_B,
                .FReactivePowerOffset = DEFAULT_FUNDAMENTAL_Q_OFFSET_B,
                .phaseOffset = DEFAULT_BASE_PHASE_B_CORRECTION,
                .IscaleFactor = DEFAULT_I_RMS_SCALE_FACTOR_B,
                .PscaleFactor = DEFAULT_P_SCALE_FACTOR_B,
            },
            .VinitialDcEstimate = DEFAULT_V_DC_ESTIMATE_B,
            .VacOffset = DEFAULT_V_AC_OFFSET_B,
            .VFundamentalAcOffset = DEFAULT_V_FUNDAMENTAL_OFFSET_B,
            .VscaleFactor = DEFAULT_V_RMS_SCALE_FACTOR_B,
        },
#endif
#if defined(THREE_PHASE_SUPPORT)
        {
            {
                .IinitialDcEstimate = DEFAULT_I_DC_ESTIMATE_C,
                .IacOffset = DEFAULT_I_AC_OFFSET_C,
                .IFAcOffset = DEFAULT_I_FUNDAMENTAL_OFFSET_C,
                .activePowerOffset = DEFAULT_P_OFFSET_C,
                .reactivePowerOffset = DEFAULT_Q_OFFSET_C,
                .FActivePowerOffset = DEFAULT_FUNDAMENTAL_P_OFFSET_C,
                .FReactivePowerOffset = DEFAULT_FUNDAMENTAL_Q_OFFSET_C,
                .phaseOffset = DEFAULT_BASE_PHASE_C_CORRECTION,
                .IscaleFactor = DEFAULT_I_RMS_SCALE_FACTOR_C,
                .PscaleFactor = DEFAULT_P_SCALE_FACTOR_C,
            },
            .VinitialDcEstimate = DEFAULT_V_DC_ESTIMATE_C,
            .VacOffset = DEFAULT_V_AC_OFFSET_C,
            .VFundamentalAcOffset = DEFAULT_V_FUNDAMENTAL_OFFSET_C,
            .VscaleFactor = DEFAULT_V_RMS_SCALE_FACTOR_C,
        },
#endif
#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT) || defined(SINGLE_PHASE_SUPPORT)
        {
            {

                .IinitialDcEstimate = DEFAULT_I_DC_ESTIMATE_A_OUT,
                .IacOffset = DEFAULT_I_AC_OFFSET_A_OUT,
                .IFAcOffset = DEFAULT_I_FUNDAMENTAL_OFFSET_A_OUT,
                .IscaleFactor = DEFAULT_I_RMS_SCALE_FACTOR_A_OUT,

                .activePowerOffset = DEFAULT_P_OFFSET_A_OUT,
                .reactivePowerOffset = DEFAULT_Q_OFFSET_A_OUT,
                .FActivePowerOffset = DEFAULT_FUNDAMENTAL_P_OFFSET_A_OUT,
                .FReactivePowerOffset = DEFAULT_FUNDAMENTAL_Q_OFFSET_A_OUT,
                .phaseOffset = DEFAULT_BASE_PHASE_A_CORRECTION_OUT,
                .PscaleFactor = DEFAULT_P_SCALE_FACTOR_A_OUT,
            },
            .VinitialDcEstimate = DEFAULT_V_DC_ESTIMATE_A_OUT,
            .VacOffset = DEFAULT_V_AC_OFFSET_A_OUT,
            .VFundamentalAcOffset = DEFAULT_V_FUNDAMENTAL_OFFSET_A_OUT,
            .VscaleFactor = DEFAULT_V_RMS_SCALE_FACTOR_A_OUT,

        },
#endif
#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT)
        {
            {
                .IinitialDcEstimate = DEFAULT_I_DC_ESTIMATE_B_OUT,
                .IacOffset = DEFAULT_I_AC_OFFSET_B_OUT,
                .IFAcOffset = DEFAULT_I_FUNDAMENTAL_OFFSET_B_OUT,
                .IscaleFactor = DEFAULT_I_RMS_SCALE_FACTOR_B_OUT,

                .activePowerOffset = DEFAULT_P_OFFSET_B_OUT,
                .reactivePowerOffset = DEFAULT_Q_OFFSET_B_OUT,
                .FActivePowerOffset = DEFAULT_FUNDAMENTAL_P_OFFSET_B_OUT,
                .FReactivePowerOffset = DEFAULT_FUNDAMENTAL_Q_OFFSET_B_OUT,
                .phaseOffset = DEFAULT_BASE_PHASE_B_CORRECTION_OUT,
                .PscaleFactor = DEFAULT_P_SCALE_FACTOR_B_OUT,

            },
            .VinitialDcEstimate = DEFAULT_V_DC_ESTIMATE_B_OUT,
            .VacOffset = DEFAULT_V_AC_OFFSET_B_OUT,
            .VFundamentalAcOffset = DEFAULT_V_FUNDAMENTAL_OFFSET_B_OUT,
            .VscaleFactor = DEFAULT_V_RMS_SCALE_FACTOR_B_OUT,
        },
#endif
#if defined(THREE_PHASE_SUPPORT)
        {
            {
                .IinitialDcEstimate = DEFAULT_I_DC_ESTIMATE_C_OUT,
                .IacOffset = DEFAULT_I_AC_OFFSET_C_OUT,
                .IFAcOffset = DEFAULT_I_FUNDAMENTAL_OFFSET_C_OUT,
                .IscaleFactor = DEFAULT_I_RMS_SCALE_FACTOR_C_OUT,

                .activePowerOffset = DEFAULT_P_OFFSET_C_OUT,
                .reactivePowerOffset = DEFAULT_Q_OFFSET_C_OUT,
                .FActivePowerOffset = DEFAULT_FUNDAMENTAL_P_OFFSET_C_OUT,
                .FReactivePowerOffset = DEFAULT_FUNDAMENTAL_Q_OFFSET_C_OUT,
                .phaseOffset = DEFAULT_BASE_PHASE_C_CORRECTION_OUT,
                .PscaleFactor = DEFAULT_P_SCALE_FACTOR_C_OUT,
            },
            .VinitialDcEstimate = DEFAULT_V_DC_ESTIMATE_C_OUT,
            .VacOffset = DEFAULT_V_AC_OFFSET_C_OUT,
            .VFundamentalAcOffset = DEFAULT_V_FUNDAMENTAL_OFFSET_C_OUT,
            .VscaleFactor = DEFAULT_V_RMS_SCALE_FACTOR_C_OUT,
        },
#endif
    },
    {
#if defined(NEUTRAL_MONITOR_SUPPORT)
        .IinitialDcEstimate = DEFAULT_I_DC_ESTIMATE_NEUTRAL,
        .IacOffset = DEFAULT_I_AC_OFFSET_NEUTRAL,
        .IFAcOffset = DEFAULT_I_FUNDAMENTAL_OFFSET_NEUTRAL,
        .activePowerOffset = DEFAULT_P_OFFSET_NEUTRAL,
        .reactivePowerOffset = DEFAULT_Q_OFFSET_NEUTRAL,
        .FActivePowerOffset = DEFAULT_FUNDAMENTAL_P_OFFSET_NEUTRAL,
        .FReactivePowerOffset = DEFAULT_FUNDAMENTAL_Q_OFFSET_NEUTRAL,
        .phaseOffset = DEFAULT_BASE_NEUTRAL_PHASE_CORRECTION,
        .IscaleFactor = DEFAULT_I_RMS_SCALE_FACTOR_NEUTRAL,
        .PscaleFactor = DEFAULT_P_SCALE_FACTOR_NEUTRAL,
#endif
    },
    .structState = 0x59,
    .calibrationInitFlag = CONFIG_INIT_FLAG};

#ifdef __cplusplus
}
#endif
#endif /* METROLOGY_CALIBRATION_DEFAULTS_H_ */
/** @}*/
