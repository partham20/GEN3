/*
 * Copyright (c) 2020 Texas Instruments Incorporated - http://www.ti.com
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

#ifndef BOARD_H
#define BOARD_H

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef __cplusplus
extern "C"
{
#endif

//
// Included Files
//

#include "driverlib.h"
#include "device.h"

//*****************************************************************************
//
// PinMux Configurations
//
//*****************************************************************************

//
// ANALOG -> myANALOGPinMux0 Pinmux
//

//
// EPWM7 -> myEPWM7 Pinmux
//
//
// EPWM7_A - GPIO Settings
//
#define GPIO_PIN_EPWM7_A 28
#define myEPWM7_EPWMA_GPIO 28
#define myEPWM7_EPWMA_PIN_CONFIG GPIO_28_EPWM7_A
//
// EPWM7_B - GPIO Settings
//
#define GPIO_PIN_EPWM7_B 29
#define myEPWM7_EPWMB_GPIO 29
#define myEPWM7_EPWMB_PIN_CONFIG GPIO_29_EPWM7_B

//*****************************************************************************
//
// ADC Configurations
//
//*****************************************************************************
#define myADC0_BASE ADCA_BASE
#define myADC0_RESULT_BASE ADCARESULT_BASE
#define myADC0_SOC0 ADC_SOC_NUMBER0
#define myADC0_FORCE_SOC0 ADC_FORCE_SOC0
#define myADC0_SAMPLE_WINDOW_SOC0 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC0 ADC_CH_ADCIN2
#define myADC0_SOC1 ADC_SOC_NUMBER1
#define myADC0_FORCE_SOC1 ADC_FORCE_SOC1
#define myADC0_SAMPLE_WINDOW_SOC1 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC1 ADC_CH_ADCIN6
#define myADC0_SOC2 ADC_SOC_NUMBER2
#define myADC0_FORCE_SOC2 ADC_FORCE_SOC2
#define myADC0_SAMPLE_WINDOW_SOC2 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC2 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC2 ADC_CH_ADCIN15
#define myADC0_SOC3 ADC_SOC_NUMBER3
#define myADC0_FORCE_SOC3 ADC_FORCE_SOC3
#define myADC0_SAMPLE_WINDOW_SOC3 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC3 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC3 ADC_CH_ADCIN0
#define myADC0_SOC4 ADC_SOC_NUMBER4
#define myADC0_FORCE_SOC4 ADC_FORCE_SOC4
#define myADC0_SAMPLE_WINDOW_SOC4 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC4 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC4 ADC_CH_ADCIN12
#define myADC0_SOC5 ADC_SOC_NUMBER5
#define myADC0_FORCE_SOC5 ADC_FORCE_SOC5
#define myADC0_SAMPLE_WINDOW_SOC5 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC5 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC5 ADC_CH_ADCIN7
#define myADC0_SOC6 ADC_SOC_NUMBER6
#define myADC0_FORCE_SOC6 ADC_FORCE_SOC6
#define myADC0_SAMPLE_WINDOW_SOC6 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC6 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC6 ADC_CH_ADCIN14
#define myADC0_SOC7 ADC_SOC_NUMBER7
#define myADC0_FORCE_SOC7 ADC_FORCE_SOC7
#define myADC0_SAMPLE_WINDOW_SOC7 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC7 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC7 ADC_CH_ADCIN3
#define myADC0_SOC8 ADC_SOC_NUMBER8
#define myADC0_FORCE_SOC8 ADC_FORCE_SOC8
#define myADC0_SAMPLE_WINDOW_SOC8 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC8 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC8 ADC_CH_ADCIN11
#define myADC0_SOC9 ADC_SOC_NUMBER9
#define myADC0_FORCE_SOC9 ADC_FORCE_SOC9
#define myADC0_SAMPLE_WINDOW_SOC9 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC9 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC9 ADC_CH_ADCIN5
#define myADC0_SOC10 ADC_SOC_NUMBER10
#define myADC0_FORCE_SOC10 ADC_FORCE_SOC10
#define myADC0_SAMPLE_WINDOW_SOC10 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC10 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC10 ADC_CH_ADCIN1
#define myADC0_SOC11 ADC_SOC_NUMBER11
#define myADC0_FORCE_SOC11 ADC_FORCE_SOC11
#define myADC0_SAMPLE_WINDOW_SOC11 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC11 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC11 ADC_CH_ADCIN8
void myADC0_init();

#define myADC1_BASE ADCB_BASE
#define myADC1_RESULT_BASE ADCBRESULT_BASE
#define myADC1_SOC0 ADC_SOC_NUMBER0
#define myADC1_FORCE_SOC0 ADC_FORCE_SOC0
#define myADC1_SAMPLE_WINDOW_SOC0 133.33333333333334
#define myADC1_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM7_SOCA
#define myADC1_CHANNEL_SOC0 ADC_CH_ADCIN2
void myADC1_init();


//*****************************************************************************
//
// ASYSCTL Configurations
//
//*****************************************************************************

//*****************************************************************************
//
// CPUTIMER Configurations
//
//*****************************************************************************
#define myCPUTIMER0_BASE CPUTIMER0_BASE
void myCPUTIMER0_init();

//*****************************************************************************
//
// EPWM Configurations
//
//*****************************************************************************
#define myEPWM7_BASE EPWM7_BASE
#define myEPWM7_TBPRD 23438
#define myEPWM7_COUNTER_MODE EPWM_COUNTER_MODE_UP
#define myEPWM7_TBPHS 0
#define myEPWM7_CMPA 11719
#define myEPWM7_CMPB 0
#define myEPWM7_CMPC 0
#define myEPWM7_CMPD 0
#define myEPWM7_DBRED 0
#define myEPWM7_DBFED 0
#define myEPWM7_TZA_ACTION EPWM_TZ_ACTION_HIGH_Z
#define myEPWM7_TZB_ACTION EPWM_TZ_ACTION_HIGH_Z
#define myEPWM7_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED

//*****************************************************************************
//
// INTERRUPT Configurations
//
//*****************************************************************************

// Interrupt Settings for INT_myADC0_1
// ISR need to be defined for the registered interrupts
#define INT_myADC0_1 INT_ADCA1
#define INT_myADC0_1_INTERRUPT_ACK_GROUP INTERRUPT_ACK_GROUP1
extern __interrupt void adcA1ISR(void);

// Interrupt Settings for INT_myCPUTIMER0
// ISR need to be defined for the registered interrupts
#define INT_myCPUTIMER0 INT_TIMER0
#define INT_myCPUTIMER0_INTERRUPT_ACK_GROUP INTERRUPT_ACK_GROUP1
extern __interrupt void INT_Cputimer_ISR(void);

//*****************************************************************************
//
// SYNC Scheme Configurations
//
//*****************************************************************************

//*****************************************************************************
//
// Board Configurations
//
//*****************************************************************************
void	Board_init();
void	ADC_init();
void	ASYSCTL_init();
void	CPUTIMER_init();
void	EPWM_init();
void	INTERRUPT_init();
void	SYNC_init();
void	PinMux_init();

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef __cplusplus
}
#endif

#endif  // end of BOARD_H definition
