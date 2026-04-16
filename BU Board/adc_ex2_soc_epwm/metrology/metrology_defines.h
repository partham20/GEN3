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
 *  @file       metrology_defines.h
 *  @brief      Defines constants enums used in Metrology module
 *
 *  @anchor metrology_defines_h
 *  # Overview
 *
 *  Defines constants enums used in Metrology module
 *
 *  <hr>
 ******************************************************************************/
/** @addtogroup Metrology
 * @{
 */

/*!
 * @file metrology_defines.h
 * @brief Constants and enumerations for the metrology system
 *
 * This file defines the constants, enumerations, and macros used throughout
 * the metrology system. It includes configuration parameters, status flags,
 * operating modes, and other essential definitions that control the behavior
 * of the power measurement system.
 *
 * The definitions support various power metering configurations including
 * single-phase, split-phase, and three-phase systems.
 */

#ifndef _METROLOGY_DEFINES_H_
#define _METROLOGY_DEFINES_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stdbool.h"
#include "math.h"
#include "template.h"
//#include "hal.h"
#include "fft.h"

#define PI 3.14159f

/*! @brief Defines Minimum Voltage ADC input in per units   */
#define V_ADC_MIN                       (-2048.0f)
/*! @brief Defines Maximum Voltage ADC input in per units   */
#define V_ADC_MAX                       (2047.0f)
/*! @brief Defines Minimum Current ADC input in per units   */
#define I_ADC_MIN                       (-2048.0f)
/*! @brief Defines Maximum Current ADC input in per units   */
#define I_ADC_MAX                       (2047.0f)
/*! @brief Defines Number of Overload hits      */
#define ENDSTOP_HITS_FOR_OVERLOAD       (20)
/*! @brief Defines Minimum Voltage Slew rate    */
#define MAX_PER_SAMPLE_VOLTAGE_SLEW     (2047.0f)
/*! @brief Defines Maximum number of FIR table elements */
#define PHASE_SHIFT_FIR_TABLE_ELEMENTS  (256)
/*! @brief Defines Voltage history array mask   */
#define V_HISTORY_MASK                  (0x3F)
/*! @brief Defines RMS Voltage over range value */
#define RMS_VOLTAGE_OVERRANGE           (0xFFFFFF)
/*! @brief Defines RMS Current over range value */
#define RMS_CURRENT_OVERRANGE           (0xFFFFFF)
/*! @brief Defines RMS Power over range value   */
#define POWER_OVERRANGE                 (0xFFFFFF)
/*! @brief Defines minimum power measured in watts  */
#define TOTAL_RESIDUAL_POWER_CUTOFF     (50.0f)
/*! @brief Defines current history samples to be stored */
#define I_HISTORY_STEPS                 (2)
/*! @brief Defines sampling rate    */
#define SAMPLES_PER_SECOND              SAMPLE_RATE

#if SAMPLE_RATE == 2000
    #define F_SAMPLE_RATE                   2000.0f
    #define INT_SAMPLES_PER_10_SECONDS      20000
#elif SAMPLE_RATE == 2930
    #define F_SAMPLE_RATE                   2929.6875f
    #define INT_SAMPLES_PER_10_SECONDS      29297
#elif SAMPLE_RATE == 3906
    #define F_SAMPLE_RATE                   3906.25f
    #define INT_SAMPLES_PER_10_SECONDS      39063
#elif SAMPLE_RATE == 4000
    #define F_SAMPLE_RATE                   4000.0f
    #define INT_SAMPLES_PER_10_SECONDS      40000
#elif SAMPLE_RATE == 5859
    #define F_SAMPLE_RATE                   5859.375f
    #define INT_SAMPLES_PER_10_SECONDS      58594
#elif SAMPLE_RATE == 6000
    #define F_SAMPLE_RATE                   6000.0f
    #define INT_SAMPLES_PER_10_SECONDS      60000
#elif SAMPLE_RATE == 6400
    #define F_SAMPLE_RATE                   6400.0f
    #define INT_SAMPLES_PER_10_SECONDS      64000
#elif SAMPLE_RATE == 7812
    #define F_SAMPLE_RATE                   7812.5f
    #define INT_SAMPLES_PER_10_SECONDS      78125
#elif SAMPLE_RATE == 8000
    #define F_SAMPLE_RATE                   8000.0f
    #define INT_SAMPLES_PER_10_SECONDS      80000
#elif SAMPLE_RATE == 6400
    #define F_SAMPLE_RATE                   6400.0f
    #define INT_SAMPLES_PER_10_SECONDS      64000
#endif

/*! @brief Defines samples per 10 seconds   */
#define SAMPLES_PER_10_SECONDS              (F_SAMPLE_RATE * 10.0f)
/*!
 * @brief Defines Energy threshold value 100 milliwatts-hr`
 *        100 milliwatt-hr = 360 watt-sec
 */
#define ENERGY_100mWATT_HOUR_THRESHOLD  (360)
/*! @brief Defines Energy threshold in IQ   */
#define ENERGY_100mWATT_HOUR_THRESHOLD_DBL  (360.0f) 
/*! @brief Mains nominal voltage in IQ15 format */
#define MAINS_NOMINAL_VOLTAGE_IQ19      (MAINS_NOMINAL_VOLTAGE)
/*! @brief Mains nominal voltage in fpu64 format */
#define MAINS_NOMINAL_VOLTAGE_DBL      (float)(MAINS_NOMINAL_VOLTAGE)
/*! @brief Mains basic current  */
#define MAINS_BASIS_CURRENT             (100)
/*! @brief Mians maximum current */
#define MAINS_MAXIMUM_CURRENT           (1000)
/*! @brief Defines sag threshold value in percentage    */
#define SAG_THRESHOLD                   (20.0f)
/*! @brief Defines sag hysteresis value in milli volts  */
#define SAG_HYSTERESIS                  (12.0f)
/*! @brief Defines swell threshold value in percentage    */
#define SWELL_THRESHOLD                 (20.0f)
/*! @brief Defines swell hysteresis value in milli volts  */
#define SWELL_HYSTERESIS                (12.0f)
/*! @brief Defines minimum voltage in milli volts to be considered as sag   */
#define MIN_SAG_VOLTAGE                 (960.0f)
/*! @brief Hysteresis voltage amount for an event to be considered a voltage interruption.  */
#define INTERRUPTION_HYSTERESIS         (120.0f)
/*! @brief Defines max frequency in 10 sec  */
#define MAINS_MAX_FREQUENCY_PER_10_SEC  MAINS_MAX_FREQUENCY / 10.0f
/*! @brief Defines min frequency in 10 sec  */
#define MAINS_MIN_FREQUENCY_PER_10_SEC  MAINS_MIN_FREQUENCY / 10.0f
/*! @brief Defines minimum samples per cycle    */
#define METROLOGY_MIN_CYCLE_SAMPLES         (256.0f * SAMPLES_PER_10_SECONDS / MAINS_MAX_FREQUENCY_PER_10_SEC)
/*! @brief Defines maximum samples per cycle    */
#define METROLOGY_MAX_CYCLE_SAMPLES         (256.0f * SAMPLES_PER_10_SECONDS / MAINS_MIN_FREQUENCY_PER_10_SEC)
/*! @brief Defines two times the samples per cycle  */
#define TWICE_CYCLES_PER_NOMINAL_FREQUENCY  ((2.0f * SAMPLES_PER_10_SECONDS)/(10.0f * MAINS_NOMINAL_FREQUENCY))
/*! @brief Defines cycles per computations  */
#if(MAINS_NOMINAL_FREQUENCY == 50)
    #define CYCLES_PER_COMPUTATION          (10)
#else
    #define CYCLES_PER_COMPUTATION          (12)
#endif
/*!
 * @brief The duration of the LED on time for an energy pulse. This is measured in ADC samples,
 *        giving a duration = (ENERGY_PULSE_DURATION - 1)/SAMPLE_RATE). The maximum allowed is 255.
 */
#define ENERGY_PULSE_DURATION               (20)
/*! @brief Defines active energy pulses per kwh */
#define TOTAL_ACTIVE_ENERGY_PULSES_PER_KW_HOUR      ENERGY_PULSE_CONSTANT
/*! @brief Defines reactive energy pulses per kwh */
#define TOTAL_REACTIVE_ENERGY_PULSES_PER_KW_HOUR    ENERGY_PULSE_CONSTANT
/*! @brief Defines active energy pulse threshlod */
#define TOTAL_ACTIVE_ENERGY_PULSE_THRESHOLD         ((float)ENERGY_100mWATT_HOUR_THRESHOLD * 1000L * SAMPLE_RATE * 10000LL)/(TOTAL_ACTIVE_ENERGY_PULSES_PER_KW_HOUR)
/*! @brief Defines reactive energy pulse threshlod */
#define TOTAL_REACTIVE_ENERGY_PULSE_THRESHOLD       ((float)ENERGY_100mWATT_HOUR_THRESHOLD * 1000L * SAMPLE_RATE * 10000LL)/(TOTAL_REACTIVE_ENERGY_PULSES_PER_KW_HOUR)

/*! @enum PHASES    */
typedef enum
{
#ifdef SINGLE_PHASE_SUPPORT
    /*! @brief Phase 1 */
    PHASE_ONE = 0,
#endif

#if defined(SPLIT_PHASE_SUPPORT) || defined(TWO_PHASE_SUPPORT)
    /*! @brief Phase 1 */
    PHASE_ONE = 0,
    /*! @brief Phase 2 */
    PHASE_TWO,
#endif

#ifdef THREE_PHASE_SUPPORT
    BRANCH_CT0 = 0,
    BRANCH_CT1,
    BRANCH_CT2,
    BRANCH_CT3,
    BRANCH_CT4,
    BRANCH_CT5,
    BRANCH_CT6,
    BRANCH_CT7,
    BRANCH_CT8,
    BRANCH_CT9,
    BRANCH_CT10,
    BRANCH_CT11,
    BRANCH_CT12,
    BRANCH_CT13,
    BRANCH_CT14,
    BRANCH_CT15,
    BRANCH_CT16,
    BRANCH_CT17,
#endif
    /*! @brief Total number of phases */
    MAX_PHASES
}PHASES;

/*! @enum METROLOGY_STATUS */
typedef enum
{
    /*! This bit indicates the meter is in the power down state. */
    METROLOGY_STATUS_POWER_DOWN = 0x0004,

    /*! This bit indicates the current status of the meter is "current flow is reversed", after
        all persistence checking, and other safeguards, have been used to check the validity of the
        reverse indication. */
    METROLOGY_STATUS_REVERSED = 0x0100,

    /*! This bit indicates the current status of the meter is "current flow is earthed", after
        all persistence checking, and other safeguards, have been used to check the validity of the
        earthed indication. */
    METROLOGY_STATUS_EARTHED = 0x0200,

    /*! This bit indicates the phase voltage is OK. */
    METROLOGY_STATUS_PHASE_VOLTAGE_OK = 0x0400,

    /*! This bit indicates the battery condition is OK. If battery monitoring is not enabled, this bit
        is not used. */
    METROLOGY_STATUS_BATTERY_OK = 0x0800
}METROLOGY_STATUS;

/*! @enum OPERATING_MODES */
typedef enum
{
    /*! The meter is operating normally*/
    OPERATING_MODE_NORMAL = 0,
#if defined(LIMP_MODE_SUPPORT)
    /*! The meter is an anti-tamper meter in limp (live only) mode */
    OPERATING_MODE_LIMP = 1,
    /*! The meter is an anti-tamper meter in limp (live only) mode, reading in short bursts */
    OPERATING_MODE_LIMP_BURST = 2,
#endif
    /*! The meter is in a battery powered state with automated meter reading, LCD and RTC functioning. */
    OPERATING_MODE_AMR_ONLY = 3,
    /*! The meter is in a battery powered state with only the LCD and RTC functioning. */
    OPERATING_MODE_LCD_ONLY = 4,
    /*! The meter is in a battery powered state with only the minimum of features (probably just the RTC) functioning. */
    OPERATING_MODE_POWERFAIL = 5
}OPERATING_MODES;

/*! @enum SAG_SWELL_EVENTS  */
typedef enum
{
    /*! @brief Voltage interruptions continuing */
    SAG_SWELL_VOLTAGE_INTERRUPTION_CONTINUING = -4,
    /*! @brief Voltage interruptions onset */
    SAG_SWELL_VOLTAGE_INTERRUPTION_ONSET      = -3,
    /*! @brief Voltage sag continuing */
    SAG_SWELL_VOLTAGE_SAG_CONTINUING          = -2,
    /*! @brief Voltage sag onset */
    SAG_SWELL_VOLTAGE_SAG_ONSET               = -1,
    /*! @brief Voltage normal */
    SAG_SWELL_VOLTAGE_NORMAL                  = 0,
    /*! @brief Voltage swell onset */
    SAG_SWELL_VOLTAGE_SWELL_ONSET             = 1,
    /*! @brief Voltage swell continuing */
    SAG_SWELL_VOLTAGE_SWELL_CONTINUING        = 2,
    /*! @brief Voltage power down */
    SAG_SWELL_VOLTAGE_POWER_DOWN_OK           = 3
}SAG_SWELL_EVENTS;

/*! @enum PHASE_STATUS */
typedef enum
{
    /*! This flag in a channel's status variable indicates there is fresh gathered data from the
        background activity to be post-processed by the foreground activity. */
    PHASE_STATUS_NEW_LOG = 0x0001,

    /*! This flag in a channel's status variable indicates the voltage signal is currently in
        the positive half of its cycle. */
    PHASE_STATUS_V_POS = 0x0002,

    /*! This flag in a channel's status variable indicates the current signal is currently in
        the positive half of its cycle. */
    PHASE_STATUS_I_POS = 0x0004,
    PHASE_STATUS_ENERGY_LOGABLE = 0x0008,

    /*! This flag in a channel's status variable indicates the voltage signal was in overload
        during the last logged interval. Overload is determined by an excessive number of
        samples hitting the end-stops of the ADC's range. */
    PHASE_STATUS_V_OVERRANGE = 0x0010,

    /*! This flag in a channel's status variable indicates the phase current signal was in overload
        during the last logged interval. Overload is determined by an excessive number of
        samples hitting the end-stops of the ADC's range. */
    PHASE_STATUS_I_OVERRANGE = 0x0020,

    /*! This flag in a channel's status variable indicates the phase current signal was reversed
        during the last logged interval. */
    PHASE_STATUS_I_REVERSED = 0x0040,

    /*! This flag in a channel's status variable indicates the phase current signal was in overload
        during the last logged interval. Overload is determined by an excessive number of
        samples hitting the end-stops of the ADC's range. This is only used if the meter supports
        monitoring of both the live and neutral leads for anti-tamper management. */
    PHASE_STATUS_I_NEUTRAL_OVERRANGE = 0x0080,

    /*! This flag in a channel's status variable indicates the neutral current signal was
        reversed during the last logged interval. This is only used if the meter supports
        monitoring of both the live and neutral leads for anti-tamper management. */
    PHASE_STATUS_I_NEUTRAL_REVERSED = 0x0100,

    /*! This flag in a channel's status variable indicates the neutral current is the one
        currently being used. This means it has been judged by the anti-tamper logic to be
        the measurement which can best be trusted. This is only used if the meter supports
        monitoring of both the live and neutral leads for anti-tamper management. */
    PHASE_STATUS_CURRENT_FROM_NEUTRAL = 0x0200,

    /*! This flag in a channel's status variable indicates the neutral current signal is
        currently in the positive half of its cycle. This is only used if the meter supports
        monitoring of both the live and neutral leads for anti-tamper management. */
    PHASE_STATUS_I_NEUTRAL_POS = 0x0800,

    /*! This flag in a channel's status variable indicates the power has been declared to be
        reversed, after the anti-tamper logic has processed the raw indications. Live neutral
        or both leads may be reversed when this bit is set. */
    PHASE_STATUS_REVERSED = 0x1000,

    /*! This flag in a channel's status variable indicates the power (current in limp mode)
        has been declared to be unbalanced, after the anti-tamper logic has processed the
        raw indications. */
    PHASE_STATUS_UNBALANCED = 0x2000,

    PHASE_STATUS_ZERO_CROSSING_MISSED = 0x4000
}PHASE_STATUS;

#ifdef __cplusplus
}
#endif
#endif /* METROLOGY_METROLOGY_DEFINES_H_ */
/** @}*/
