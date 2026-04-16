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
#define GPIO_PIN_EPWM7_A 12
#define myEPWM7_EPWMA_GPIO 12
#define myEPWM7_EPWMA_PIN_CONFIG GPIO_12_EPWM7_A
//
// EPWM7_B - GPIO Settings
//
#define GPIO_PIN_EPWM7_B 29
#define myEPWM7_EPWMB_GPIO 29
#define myEPWM7_EPWMB_PIN_CONFIG GPIO_29_EPWM7_B
//
// GPIO28 - GPIO Settings
//
#define myGPIO28_GPIO_PIN_CONFIG GPIO_28_GPIO28

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
#define myADC0_CHANNEL_SOC0 ADC_CH_ADCIN6
#define myADC0_SOC1 ADC_SOC_NUMBER1
#define myADC0_FORCE_SOC1 ADC_FORCE_SOC1
#define myADC0_SAMPLE_WINDOW_SOC1 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC1 ADC_CH_ADCIN2
#define myADC0_SOC2 ADC_SOC_NUMBER2
#define myADC0_FORCE_SOC2 ADC_FORCE_SOC2
#define myADC0_SAMPLE_WINDOW_SOC2 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC2 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC2 ADC_CH_ADCIN11
#define myADC0_SOC3 ADC_SOC_NUMBER3
#define myADC0_FORCE_SOC3 ADC_FORCE_SOC3
#define myADC0_SAMPLE_WINDOW_SOC3 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC3 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC3 ADC_CH_ADCIN12
#define myADC0_SOC4 ADC_SOC_NUMBER4
#define myADC0_FORCE_SOC4 ADC_FORCE_SOC4
#define myADC0_SAMPLE_WINDOW_SOC4 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC4 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC4 ADC_CH_ADCIN4
#define myADC0_SOC5 ADC_SOC_NUMBER5
#define myADC0_FORCE_SOC5 ADC_FORCE_SOC5
#define myADC0_SAMPLE_WINDOW_SOC5 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC5 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC5 ADC_CH_ADCIN15
#define myADC0_SOC6 ADC_SOC_NUMBER6
#define myADC0_FORCE_SOC6 ADC_FORCE_SOC6
#define myADC0_SAMPLE_WINDOW_SOC6 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC6 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC6 ADC_CH_ADCIN5
#define myADC0_SOC7 ADC_SOC_NUMBER7
#define myADC0_FORCE_SOC7 ADC_FORCE_SOC7
#define myADC0_SAMPLE_WINDOW_SOC7 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC7 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC7 ADC_CH_ADCIN7
#define myADC0_SOC8 ADC_SOC_NUMBER8
#define myADC0_FORCE_SOC8 ADC_FORCE_SOC8
#define myADC0_SAMPLE_WINDOW_SOC8 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC8 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC8 ADC_CH_ADCIN9
#define myADC0_SOC9 ADC_SOC_NUMBER9
#define myADC0_FORCE_SOC9 ADC_FORCE_SOC9
#define myADC0_SAMPLE_WINDOW_SOC9 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC9 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC9 ADC_CH_ADCIN3
#define myADC0_SOC10 ADC_SOC_NUMBER10
#define myADC0_FORCE_SOC10 ADC_FORCE_SOC10
#define myADC0_SAMPLE_WINDOW_SOC10 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC10 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC10 ADC_CH_ADCIN14
#define myADC0_SOC11 ADC_SOC_NUMBER11
#define myADC0_FORCE_SOC11 ADC_FORCE_SOC11
#define myADC0_SAMPLE_WINDOW_SOC11 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC11 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC11 ADC_CH_ADCIN1
#define myADC0_SOC12 ADC_SOC_NUMBER12
#define myADC0_FORCE_SOC12 ADC_FORCE_SOC12
#define myADC0_SAMPLE_WINDOW_SOC12 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC12 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC12 ADC_CH_ADCIN8
#define myADC0_SOC13 ADC_SOC_NUMBER13
#define myADC0_FORCE_SOC13 ADC_FORCE_SOC13
#define myADC0_SAMPLE_WINDOW_SOC13 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC13 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC13 ADC_CH_ADCIN24
#define myADC0_SOC14 ADC_SOC_NUMBER14
#define myADC0_FORCE_SOC14 ADC_FORCE_SOC14
#define myADC0_SAMPLE_WINDOW_SOC14 133.33333333333334
#define myADC0_TRIGGER_SOURCE_SOC14 ADC_TRIGGER_EPWM7_SOCA
#define myADC0_CHANNEL_SOC14 ADC_CH_ADCIN25
void myADC0_init();

#define myADC1_BASE ADCE_BASE
#define myADC1_RESULT_BASE ADCERESULT_BASE
#define myADC1_SOC0 ADC_SOC_NUMBER0
#define myADC1_FORCE_SOC0 ADC_FORCE_SOC0
#define myADC1_SAMPLE_WINDOW_SOC0 133.33333333333334
#define myADC1_TRIGGER_SOURCE_SOC0 ADC_TRIGGER_EPWM7_SOCA
#define myADC1_CHANNEL_SOC0 ADC_CH_ADCIN1
#define myADC1_SOC1 ADC_SOC_NUMBER1
#define myADC1_FORCE_SOC1 ADC_FORCE_SOC1
#define myADC1_SAMPLE_WINDOW_SOC1 133.33333333333334
#define myADC1_TRIGGER_SOURCE_SOC1 ADC_TRIGGER_EPWM7_SOCA
#define myADC1_CHANNEL_SOC1 ADC_CH_ADCIN12
#define myADC1_SOC2 ADC_SOC_NUMBER2
#define myADC1_FORCE_SOC2 ADC_FORCE_SOC2
#define myADC1_SAMPLE_WINDOW_SOC2 133.33333333333334
#define myADC1_TRIGGER_SOURCE_SOC2 ADC_TRIGGER_EPWM7_SOCA
#define myADC1_CHANNEL_SOC2 ADC_CH_ADCIN2
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
// DAC Configurations
//
//*****************************************************************************
#define myDAC0_BASE DACA_BASE
void myDAC0_init();

//*****************************************************************************
//
// DMA Configurations
//
//*****************************************************************************
#define myDMA0_SRCADDRESS 2816 
#define myDMA0_DESTADDRESS 0 
#define myDMA0_BASE DMA_CH1_BASE 
#define myDMA0_BURSTSIZE 1U
#define myDMA0_TRANSFERSIZE 1028U
#define myDMA0_SRC_WRAPSIZE 65535U
#define myDMA0_DEST_WRAPSIZE 65535U
void myDMA0_init();

//*****************************************************************************
//
// EPWM Configurations
//
//*****************************************************************************
#define myEPWM7_BASE EPWM7_BASE
#define myEPWM7_TBPRD 23438
#define myEPWM7_COUNTER_MODE EPWM_COUNTER_MODE_STOP_FREEZE
#define myEPWM7_TBPHS 300
#define myEPWM7_CMPA 11719
#define myEPWM7_CMPB 0
#define myEPWM7_CMPC 0
#define myEPWM7_CMPD 0
#define myEPWM7_DBRED 0
#define myEPWM7_DBFED 0
#define myEPWM7_TZA_ACTION EPWM_TZ_ACTION_LOW
#define myEPWM7_TZB_ACTION EPWM_TZ_ACTION_HIGH
#define myEPWM7_TZ_INTERRUPT_SOURCES EPWM_TZ_INTERRUPT_OST
#define myEPWM7_INTERRUPT_SOURCE EPWM_INT_TBCTR_DISABLED

//*****************************************************************************
//
// GPIO Configurations
//
//*****************************************************************************
#define myGPIO28 28
void myGPIO28_init();

//*****************************************************************************
//
// INPUTXBAR Configurations
//
//*****************************************************************************
#define myINPUTXBARINPUT0_SOURCE 28
#define myINPUTXBARINPUT0_INPUT XBAR_INPUT5
void myINPUTXBARINPUT0_init();

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
extern __interrupt void cpuTimer0ISR(void);

// Interrupt Settings for INT_myGPIO28_XINT
// ISR need to be defined for the registered interrupts
#define INT_myGPIO28_XINT INT_XINT2
#define INT_myGPIO28_XINT_INTERRUPT_ACK_GROUP INTERRUPT_ACK_GROUP1
extern __interrupt void GPIOISR(void);

//*****************************************************************************
//
// SYNC Scheme Configurations
//
//*****************************************************************************

//*****************************************************************************
//
// XINT Configurations
//
//*****************************************************************************
#define myGPIO28_XINT GPIO_INT_XINT2
#define myGPIO28_XINT_TYPE GPIO_INT_TYPE_FALLING_EDGE
void myGPIO28_XINT_init();

//*****************************************************************************
//
// Board Configurations
//
//*****************************************************************************
void	Board_init();
void	ADC_init();
void	ASYSCTL_init();
void	CPUTIMER_init();
void	DAC_init();
void	DMA_init();
void	EPWM_init();
void	GPIO_init();
void	INPUTXBAR_init();
void	INTERRUPT_init();
void	SYNC_init();
void	XINT_init();
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
