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
 *  @file       template.h
 *  @brief      TIDA_010243 application template
 *
 *
 *  @anchor TIDA_010243_template_h
 *  # Overview
 *    Defines the inputs to metrology, ADS modules as per hardware design, application
 *    requirements in float.
 *
 *  <hr>
 ******************************************************************************/
/** @addtogroup TIDA_010243
 * @{
 */

#ifndef TEMPLATE_H
#define TEMPLATE_H

#ifdef __cplusplus
extern "C" {
#endif

/*! @brief Defines TIDA_010243 design   */
#define TIDA_010243
/*! @brief Defines Single phase measurement support */
#undef SINGLE_PHASE_SUPPORT
/*! @brief Defines Two phase measurement support */
#undef TWO_PHASE_SUPPORT
/*! @brief Defines Three phase measurement support */
#define THREE_PHASE_SUPPORT
/*! @brief Defines Split phase measurement support */
#undef SPLIT_PHASE_SUPPORT
#ifdef SPLIT_PHASE_SUPPORT
/*! @brief Defines split phase single voltage configuration */
#undef SPLIT_PHASE_SINGLE_VOLTAGE_CONFIGURATION
/*! @brief Defines split phase two voltage configuration */
#undef SPLIT_PHASE_TWO_VOLTAGE_CONFIGURATION
/*! @brief Defines split phase star configuration */
#undef SPLIT_PHASE_STAR_CONFIGURATION
/*! @brief Defines split phase delta configuration */
#undef SPLIT_PHASE_DELTA_CONFIGURATION
#endif
/*! @brief Defines neutral monitor support */
#define NEUTRAL_MONITOR_SUPPORT
/*! @brief Defines Rogoswki coil sensing support */
#undef ROGOWSKI_SUPPORT
/*! @brief Defines ADS channel count    */
#define ADS_CHANNELCOUNT        (8)
//#define HARMONICS_SUPPORT
/*!
 *  @brief Defines sample rate
 *  Accepted values 2000, 2930, 3906, 4000, 5859, 6000, 6400, 7812, 8000
 */
#define SAMPLE_RATE             (6400)
/*! @brief Defines Mains Nominal Frequency in Hz */
#define MAINS_NOMINAL_FREQUENCY                 50
/*! @brief Defines Mains Minimum Frequency in Hz */
#define MAINS_MIN_FREQUENCY                     40.0f
/*! @brief Defines Mains Maximum Frequency in Hz */
#define MAINS_MAX_FREQUENCY                     70.0f
/*! @brief Defines Mains Nominal Voltage in volts*/
#define MAINS_NOMINAL_VOLTAGE                  2048.0f
/*! @brief Defines Energy pulse constant         */
#define ENERGY_PULSE_CONSTANT                 6400.0f
/*! @brief Defines fundamental parameters support   */
#define FUNDAMENTAL_PARAMETERS
/*! @brief Defines Vrms Support */
#define VRMS_SUPPORT
/*! @brief Defines Irms Support */
#define IRMS_SUPPORT
/*! @brief Defines Active Power Support   */
#define ACTIVE_POWER_SUPPORT
/*! @brief Defines Reactive Power Support */
#define REACTIVE_POWER_SUPPORT
/*! @brief Defines Apparent Power Support */
#define APPARENT_POWER_SUPPORT
/*! @brief Defines Active Energy Support */
#define ACTIVE_ENERGY_SUPPORT
/*! @brief Defines Reactive Energy Support */
#define REACTIVE_ENERGY_SUPPORT
/*! @brief Defines Apparent Energy Support */
#define APPARENT_ENERGY_SUPPORT
/*! @brief Defines Total Active Power Support */
#define TOTAL_ACTIVE_POWER_SUPPORT
/*! @brief Defines Total Reactive Power Support */
#define TOTAL_REACTIVE_POWER_SUPPORT
/*! @brief Defines Total Apparent Power Support */
#define TOTAL_APPARENT_POWER_SUPPORT
/*! @brief Defines Total Active Energy Support */
#define TOTAL_ACTIVE_ENERGY_SUPPORT
/*! @brief Defines Total Reactive Energy Support */
#define TOTAL_REACTIVE_ENERGY_SUPPORT
/*! @brief Defines Total Apparent Energy Support */
#define TOTAL_APPARENT_ENERGY_SUPPORT
/*! @brief Defines Power factor Support */
#define POWER_FACTOR_SUPPORT
/*! @brief Defines Power factor angle Support */
#define POWER_FACTOR_ANGLE_SUPPORT
/*! @brief Defines voltage under deviation Support */
#define VOLTAGE_UNDER_DEVIATION
/*! @brief Defines voltage over deviation Support */
#define VOLTAGE_OVER_DEVIATION
/*! @brief Defines sag swell Support */
#define SAG_SWELL_SUPPORT
/*! @brief Defines frequency Support */
#define FREQUENCY_SUPPORT
#ifdef FUNDAMENTAL_PARAMETERS
/*! @brief Defines Fundamental Vrms Support */
#define FUNDAMENTAL_VRMS_SUPPORT
/*! @brief Defines Fundamental Irms Support */
#define FUNDAMENTAL_IRMS_SUPPORT
/*! @brief Defines Fundamental Active Power Support   */
#define FUNDAMENTAL_ACTIVE_POWER_SUPPORT
/*! @brief Defines Fundamental Reactive Power Support */
#define FUNDAMENTAL_REACTIVE_POWER_SUPPORT
/*! @brief Defines Fundamental Apparent Power Support */
#define FUNDAMENTAL_APPARENT_POWER_SUPPORT
/*! @brief Defines Fundamental Active Energy Support   */
#define FUNDAMENTAL_ACTIVE_ENERGY_SUPPORT
/*! @brief Defines Fundamental Reactive Energy Support */
#define FUNDAMENTAL_REACTIVE_ENERGY_SUPPORT
/*! @brief Defines Fundamental Apparent Energy Support */
#define FUNDAMENTAL_APPARENT_ENERGY_SUPPORT
/*! @brief Defines voltage THD Support */
#define VOLTAGE_THD_SUPPORT
/*! @brief Defines current THD Support */
#define CURRENT_THD_SUPPORT
/*! @brief Defines Total Fundamental Active Power Support */
#define TOTAL_FUNDAMENTAL_ACTIVE_POWER_SUPPORT
/*! @brief Defines Total Fundamental Reactive Power Support */
#define TOTAL_FUNDAMENTAL_REACTIVE_POWER_SUPPORT
/*! @brief Defines Total Fundamental Apparent Power Support */
#define TOTAL_FUNDAMENTAL_APPARENT_POWER_SUPPORT
/*! @brief Defines Total Fundamental Active Energy Support */
#define TOTAL_FUNDAMENTAL_ACTIVE_ENERGY_SUPPORT
/*! @brief Defines Total Fundamental Reactive Energy Support */
#define TOTAL_FUNDAMENTAL_REACTIVE_ENERGY_SUPPORT
/*! @brief Defines Total Fundamental Apparent Energy Support */
#define TOTAL_FUNDAMENTAL_APPARENT_ENERGY_SUPPORT
#endif
#ifdef THREE_PHASE_SUPPORT
/*! @brief Defines Line to Line voltage Support */
#define LINETOLINE_VOLTAGE_SUPPORT
/*! @brief Defines Fundamental Line to Line voltage Support */
#define FUNDAMENTAL_LINETOLINE_VOLTAGE_SUPPORT
/*! @brief Defines Aggregate power factor Support */
#define AGGREGATE_POWER_FACTOR
/*! @brief Defines Current vector sum Support */
#define CURRENT_VECTOR_SUM
/*! @brief Defines Phase to Phase angle Support */
#define PHASE_TO_PHASE_ANGLE_SUPPORT
#endif

#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT) || defined(SINGLE_PHASE_SUPPORT)
/*!
 * @brief Defines the voltage scale factor for phase A
 *        It depends on hardware design and ADS/AMC configuration
 */
#define PHASE_A_VOLTAGE_SCALE_FACTOR                    20.8777771f////21.05555f
//Secondary
#define PHASE_A_VOLTAGE_SCALE_FACTOR_OUT                20.8777771f//75.51111f
/*!
 * @brief Defines the current scale factor for phase A
 *        It depends on hardware design and ADS/AMC configuration
 */



//secondry
#define PHASE_A_CURRENT_DC_ESTIMATE_OUT                 0.0f
#define PHASE_A_CURRENT_AC_OFFSET_OUT                  0.0f
#define PHASE_A_CURRENT_FUNDAMENTAL_OFFSET_OUT         0.0f
#define PHASE_A_CURRENT_SCALE_FACTOR_OUT                    73.4000015f//4.89999008f

#define PHASE_B_CURRENT_DC_ESTIMATE_OUT                 0.0f
#define PHASE_B_CURRENT_AC_OFFSET_OUT                   0.0f
#define PHASE_B_CURRENT_FUNDAMENTAL_OFFSET_OUT         0.0f
#define PHASE_B_CURRENT_SCALE_FACTOR_OUT                    73.4000015f//4.89999008f

#define PHASE_C_CURRENT_DC_ESTIMATE_OUT                 0.0f
#define PHASE_C_CURRENT_AC_OFFSET_OUT                   0.0f
#define PHASE_C_CURRENT_FUNDAMENTAL_OFFSET_OUT         0.0f
#define PHASE_C_CURRENT_SCALE_FACTOR_OUT                    73.4000015f//4.89999008f

#define POWER_GAIN_FACTOR                             0.0001f


#define PHASE_A_CURRENT_SCALE_FACTOR                    35.4000015f //4.89999008f
/*!
 * @brief Defines the power scale factor for phase A
 *        It is product of voltage and current scale factor
 */
#define PHASE_A_POWER_SCALE_FACTOR                  (PHASE_A_VOLTAGE_SCALE_FACTOR * PHASE_A_CURRENT_SCALE_FACTOR*POWER_GAIN_FACTOR)
/*!
 * @brief Defines estimate for the DC level of the voltage channel of phase A.
 *  Using this estimate helps reduce the initial settling time of the DC
 *  voltage filter for phase A
 */
#define PHASE_A_VOLTAGE_DC_ESTIMATE                     0.0f
//Secondary
#define PHASE_A_VOLTAGE_DC_ESTIMATE_OUT                 100.0f
/*!
 * @brief Defines estimate for the DC level of the current channel of phase A.
 *  Using this estimate helps reduce the initial settling time of the DC
 *  current filter for phase A
 */
#define PHASE_A_CURRENT_DC_ESTIMATE                 0.0f
/*!
 * @brief Defines the offset in AC voltage, this value is subtracted from the
 * computed voltage in phase A.
 */
#define PHASE_A_VOLTAGE_AC_OFFSET                     0.0f
//Secondary
#define PHASE_A_VOLTAGE_AC_OFFSET_OUT                 0.0f
/*!
 * @brief Defines the offset in AC fundamental voltage, this value is
 * subtracted from the computed voltage in phase A.
 */
#define PHASE_A_VOLTAGE_FUNDAMENTAL_OFFSET         0.0f
//Secondary
#define PHASE_A_VOLTAGE_FUNDAMENTAL_OFFSET_OUT         0.0f
/*!
 * @brief Defines the offset in AC current, this value is subtracted from the
 * computed current in phase A.
 */
#define PHASE_A_CURRENT_AC_OFFSET                   360.0f
/*!
 * @brief Defines the offset in AC fundamental current, this value is
 * subtracted from the computed current in phase A.
 */
#define PHASE_A_CURRENT_FUNDAMENTAL_OFFSET         0.0f
/*!
 * @brief Defines the offset in active power, this value is subtracted from
 * the computed active power in phase A.
 */
#define PHASE_A_ACTIVE_POWER_OFFSET                 0.0f
/*!
 * @brief Defines the offset in fundamental active power, this value is
 * subtracted from the computed active power in phase A.
 */
#define PHASE_A_ACTIVE_POWER_FUNDAMENTAL_OFFSET    0.0f
/*!
 * @brief Defines the offset in reactive power, this value is subtracted from
 * the computed active power in phase A.
 */
#define PHASE_A_REACTIVE_POWER_OFFSET               0.0f
/*!
 * @brief Defines the offset in fundamental reactive power, this value is
 * subtracted from the computed reactive power in phase A.
 */
#define PHASE_A_REACTIVE_POWER_FUNDAMENTAL_OFFSET  0.0f
#endif

#if defined(THREE_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT) || defined(SPLIT_PHASE_SUPPORT)
/*!
 * @brief Defines the voltage scale factor for phase B
 *        It depends on hardware design and ADS/AMC configuration
 */
#define PHASE_B_VOLTAGE_SCALE_FACTOR                    21.2579994f////20.54444f
//Secondary35.4000015
#define PHASE_B_VOLTAGE_SCALE_FACTOR_OUT                20.5579994f//63.77777f
/*!
 * @brief Defines the current scale factor for phase B
 *        It depends on hardware design and ADS/AMC configuration
 */
#define PHASE_B_CURRENT_SCALE_FACTOR                    73.4000015f//4.89999008f
/*!
 * @brief Defines the power scale factor for phase B
 */
#define PHASE_B_POWER_SCALE_FACTOR                  (PHASE_B_VOLTAGE_SCALE_FACTOR * PHASE_B_CURRENT_SCALE_FACTOR*POWER_GAIN_FACTOR)
/*!
 * @brief Defines estimate for the DC level of the voltage channel of phase B.
 *  Using this estimate helps reduce the initial settling time of the DC
 *  voltage filter for phase B
 */
#define PHASE_B_VOLTAGE_DC_ESTIMATE                     100.0f
//Secondary
#define PHASE_B_VOLTAGE_DC_ESTIMATE_OUT                 100.0f

/*!
 * @brief Defines estimate for the DC level of the current channel of phase B.
 *  Using this estimate helps reduce the initial settling time of the DC
 *  current filter for phase B
 */
#define PHASE_B_CURRENT_DC_ESTIMATE                 0.0f
/*!
 * @brief Defines the offset in AC voltage, this value is subtracted from the
 * computed voltage in phase B.
 */
#define PHASE_B_VOLTAGE_AC_OFFSET                   0.0f
//Secondary
#define PHASE_B_VOLTAGE_AC_OFFSET_OUT                   0.0f
/*!
 * @brief Defines the offset in AC fundamental voltage, this value is
 * subtracted from the computed voltage in phase B.
 */
#define PHASE_B_VOLTAGE_FUNDAMENTAL_OFFSET             0.0f
//Secondary
#define PHASE_B_VOLTAGE_FUNDAMENTAL_OFFSET_OUT         0.0f
/*!
 * @brief Defines the offset in AC current, this value is subtracted from the
 * computed current in phase B.
 */
#define PHASE_B_CURRENT_AC_OFFSET                   360.0f
/*!
 * @brief Defines the offset in AC fundamental current, this value is
 * subtracted from the computed current in phase B.
 */
#define PHASE_B_CURRENT_FUNDAMENTAL_OFFSET         0.0f
/*!
 * @brief Defines the offset in active power, this value is subtracted from
 * the computed active power in phase B.
 */
#define PHASE_B_ACTIVE_POWER_OFFSET                 0.0f
/*!
 * @brief Defines the offset in fundamental active power, this value is
 * subtracted from the computed active power in phase B.
 */
#define PHASE_B_ACTIVE_POWER_FUNDAMENTAL_OFFSET    0.0f
/*!
 * @brief Defines the offset in reactive power, this value is subtracted from
 * the computed active power in phase B.
 */
#define PHASE_B_REACTIVE_POWER_OFFSET               0.0f
/*!
 * @brief Defines the offset in fundamental reactive power, this value is
 * subtracted from the computed reactive power in phase A.
 */
#define PHASE_B_REACTIVE_POWER_FUNDAMENTAL_OFFSET  0.0f
#endif

#if defined(THREE_PHASE_SUPPORT)
/*!
 * @brief Defines the voltage scale factor for phase C
 *        It depends on hardware design and ADS/AMC configuration
 */
#define PHASE_C_VOLTAGE_SCALE_FACTOR                    21.2221107f//21.44444f
//Secondary
#define PHASE_C_VOLTAGE_SCALE_FACTOR_OUT                21.2222214f//21.7777f

/*!
 * @brief Defines the current scale factor for phase B
 *        It depends on hardware design and ADS/AMC configuration
 */
#define PHASE_C_CURRENT_SCALE_FACTOR               73.4000015f//4.89999008f
/*!
 * @brief Defines the power scale factor for phase
 */
#define PHASE_C_POWER_SCALE_FACTOR                  (PHASE_C_VOLTAGE_SCALE_FACTOR * PHASE_C_CURRENT_SCALE_FACTOR*POWER_GAIN_FACTOR)
/*!
 * @brief Defines estimate for the DC level of the voltage channel of phase C.
 *  Using this estimate helps reduce the initial settling time of the DC
 *  voltage filter for phase C
 */
#define PHASE_C_VOLTAGE_DC_ESTIMATE                     100.0f
//Secondary
#define PHASE_C_VOLTAGE_DC_ESTIMATE_OUT                 100.0f

/*!
 * @brief Defines estimate for the DC level of the current channel of phase C.
 *  Using this estimate helps reduce the initial settling time of the DC
 *  current filter for phase C
 */
#define PHASE_C_CURRENT_DC_ESTIMATE                 0.0f
/*!
 * @brief Defines the offset in AC voltage, this value is subtracted from the
 * computed voltage in phase C.
 */
#define PHASE_C_VOLTAGE_AC_OFFSET                       0.0f
//Secondary
#define PHASE_C_VOLTAGE_AC_OFFSET_OUT                   0.0f
/*!
 * @brief Defines the offset in AC fundamental voltage, this value is
 * subtracted from the computed voltage in phase C.
 */
#define PHASE_C_VOLTAGE_FUNDAMENTAL_OFFSET             0.0f
//Secondary
#define PHASE_C_VOLTAGE_FUNDAMENTAL_OFFSET_OUT         0.0f
/*!
 * @brief Defines the offset in AC current, this value is subtracted from the
 * computed current in phase C.
 */
#define PHASE_C_CURRENT_AC_OFFSET                   360.0f
/*!
 * @brief Defines the offset in AC fundamental current, this value is
 * subtracted from the computed current in phase C.
 */
#define PHASE_C_CURRENT_FUNDAMENTAL_OFFSET         0.0f
/*!
 * @brief Defines the offset in active power, this value is subtracted from
 * the computed active power in phase C.
 */
#define PHASE_C_ACTIVE_POWER_OFFSET                 0.0f
//secondry
#define PHASE_A_ACTIVE_POWER_OFFSET_OUT                 0.0f
#define PHASE_B_ACTIVE_POWER_OFFSET_OUT                 0.0f
#define PHASE_C_ACTIVE_POWER_OFFSET_OUT                 0.0f

#define PHASE_A_REACTIVE_POWER_OFFSET_OUT               0.0f
#define PHASE_B_REACTIVE_POWER_OFFSET_OUT               0.0f
#define PHASE_C_REACTIVE_POWER_OFFSET_OUT               0.0f

#define PHASE_A_ACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT    0.0f
#define PHASE_B_ACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT    0.0f
#define PHASE_C_ACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT    0.0f

#define PHASE_A_REACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT    0.0f
#define PHASE_B_REACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT    0.0f
#define PHASE_C_REACTIVE_POWER_FUNDAMENTAL_OFFSET_OUT    0.0f

#define PHASE_A_POWER_SCALE_FACTOR_OUT                  (PHASE_A_VOLTAGE_SCALE_FACTOR_OUT * PHASE_A_CURRENT_SCALE_FACTOR_OUT*POWER_GAIN_FACTOR)
#define PHASE_B_POWER_SCALE_FACTOR_OUT                  (PHASE_B_VOLTAGE_SCALE_FACTOR_OUT * PHASE_B_CURRENT_SCALE_FACTOR_OUT*POWER_GAIN_FACTOR)
#define PHASE_C_POWER_SCALE_FACTOR_OUT                  (PHASE_C_VOLTAGE_SCALE_FACTOR_OUT * PHASE_C_CURRENT_SCALE_FACTOR_OUT*POWER_GAIN_FACTOR)

/*!
 * @brief Defines the offset in fundamental active power, this value is
 * subtracted from the computed active power in phase C.
 */
#define PHASE_C_ACTIVE_POWER_FUNDAMENTAL_OFFSET    0.0f
/*!
 * @brief Defines the offset in reactive power, this value is subtracted from
 * the computed active power in phase C.
 */
#define PHASE_C_REACTIVE_POWER_OFFSET               0.0f
/*!
 * @brief Defines the offset in fundamental reactive power, this value is
 * subtracted from the computed reactive power in phase C.
 */
#define PHASE_C_REACTIVE_POWER_FUNDAMENTAL_OFFSET  0.0f
#endif

#if defined(NEUTRAL_MONITOR_SUPPORT)
/*!
 * @brief Defines the current scale factor for neutral
 *        It depends on hardware design and ADS/AMC configuration
 */
#define NEUTRAL_CURRENT_SCALE_FACTOR                125.0f
/*!
 * @brief Defines estimate for the DC level of the current channel of neutral.
 *  Using this estimate helps reduce the initial settling time of the DC
 *  current filter for neutral
 */
#define NEUTRAL_CURRENT_DC_ESTIMATE                 0.0f
/*!
 * @brief Defines the offset in AC current, this value is subtracted from the
 * computed current in neutral.
 */
#define NEUTRAl_CURRENT_AC_OFFSET                   0.0f
/*!
 * @brief Defines the offset in AC fundamental current, this value is
 * subtracted from the computed current in neutral.
 */
#define NEUTRAL_CURRENT_FUNDAMENTAL_OFFSET         0.0f
/*!
 * @brief Defines the power scale factor for neutral
 */
#define NEUTRAL_POWER_SCALE_FACTOR                  125.0f
/*!
 * @brief Defines the offset in active power, this value is subtracted from
 * the computed active power in neutral.
 */
#define NEUTRAL_ACTIVE_POWER_OFFSET                 0.0f
/*!
 * @brief Defines the offset in reactive power, this value is subtracted from
 * the computed active power in neutral.
 */
#define NEUTRAL_REACTIVE_POWER_OFFSET               0.0f
/*!
 * @brief Defines the offset in fundamental active power, this value is
 * subtracted from the computed active power in neutral.
 */
#define NEUTRAL_ACTIVE_POWER_FUNDAMENTAL_OFFSET     0.0f
/*!
 * @brief Defines the offset in fundamental reactive power, this value is
 * subtracted from the computed reactive power in neutral.
 */
#define NEUTRAL_REACTIVE_POWER_FUNDAMENTAL_OFFSET   0.0f
#endif

#ifdef __cplusplus
}
#endif
#endif /* TEMPLATE_H */
/** @}*/
