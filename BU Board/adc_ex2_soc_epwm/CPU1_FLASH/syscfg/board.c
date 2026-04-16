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

#include "board.h"

//*****************************************************************************
//
// Board Configurations
// Initializes the rest of the modules. 
// Call this function in your application if you wish to do all module 
// initialization.
// If you wish to not use some of the initializations, instead of the 
// Board_init use the individual Module_inits
//
//*****************************************************************************
void Board_init()
{
	EALLOW;

	PinMux_init();
	INPUTXBAR_init();
	SYNC_init();
	ASYSCTL_init();
	ADC_init();
	CPUTIMER_init();
	DAC_init();
	DMA_init();
	EPWM_init();
	GPIO_init();
	XINT_init();
	INTERRUPT_init();

	EDIS;
}

//*****************************************************************************
//
// PINMUX Configurations
//
//*****************************************************************************
void PinMux_init()
{
	//
	// PinMux for modules assigned to CPU1
	//
	
	//
	// ANALOG -> myANALOGPinMux0 Pinmux
	//
	// Analog PinMux for A0, B15, C15, DACA_OUT
	GPIO_setPinConfig(GPIO_231_GPIO231);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(231, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A2, B6, C9, PGA1_INP, GPIO224
	GPIO_setPinConfig(GPIO_224_GPIO224);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(224, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A3, B3, PGA2_INP, C5, GPIO242
	GPIO_setPinConfig(GPIO_242_GPIO242);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(242, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A4, B8, C14
	GPIO_setPinConfig(GPIO_225_GPIO225);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(225, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A5, B12, C2, PGA2_INM
	GPIO_setPinConfig(GPIO_244_GPIO244);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(244, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A6, D14, E14, GPIO228
	GPIO_setPinConfig(GPIO_228_GPIO228);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(228, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A7, B30, C3, D12, E30, PGA3_INM
	GPIO_setPinConfig(GPIO_245_GPIO245);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(245, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A8, B0, C11, PGA3_OUT
	GPIO_setPinConfig(GPIO_241_GPIO241);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(241, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A9, B4, C8, GPIO227, GPIO236
	GPIO_setPinConfig(GPIO_236_GPIO236);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(236, GPIO_ANALOG_ENABLED);
	// Multi-bonded AGPIO, unused GPIO is also configured in analog mode
	GPIO_setPinConfig(GPIO_227_GPIO227);
	GPIO_setAnalogMode(227, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A11, B10, C0, PGA2_OUT
	GPIO_setPinConfig(GPIO_237_GPIO237);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(237, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A12, C1, E11, PGA3_INP
	GPIO_setPinConfig(GPIO_238_GPIO238);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(238, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A14, B14, C4, PGA1_OUT
	GPIO_setPinConfig(GPIO_239_GPIO239);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(239, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A15, B9, C7, PGA1_INM
	GPIO_setPinConfig(GPIO_233_GPIO233);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(233, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A24, D0, E0, GPIO11
	GPIO_setPinConfig(GPIO_11_GPIO11);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(11, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A25, D3, E3, GPIO17
	GPIO_setPinConfig(GPIO_17_GPIO17);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(17, GPIO_ANALOG_ENABLED);
	// Analog PinMux for B2, C6, E12, GPIO226
	GPIO_setPinConfig(GPIO_226_GPIO226);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(226, GPIO_ANALOG_ENABLED);
	// Analog PinMux for B24, D1, E1, GPIO33
	GPIO_setPinConfig(GPIO_33_GPIO33);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(33, GPIO_ANALOG_ENABLED);
	// Analog PinMux for C24, D2, E2, GPIO16
	GPIO_setPinConfig(GPIO_16_GPIO16);
	// AGPIO -> Analog mode selected
	GPIO_setAnalogMode(16, GPIO_ANALOG_ENABLED);
	// Analog PinMux for A1, B7, D11, CMP1_DACL
	GPIO_setPinConfig(GPIO_232_GPIO232);
	// AIO -> Analog mode selected
	GPIO_setAnalogMode(232, GPIO_ANALOG_ENABLED);
	//
	// EPWM7 -> myEPWM7 Pinmux
	//
	GPIO_setPinConfig(myEPWM7_EPWMA_PIN_CONFIG);
	// AGPIO -> GPIO mode selected
	GPIO_setAnalogMode(12, GPIO_ANALOG_DISABLED);
	GPIO_setPadConfig(myEPWM7_EPWMA_GPIO, GPIO_PIN_TYPE_STD);
	GPIO_setQualificationMode(myEPWM7_EPWMA_GPIO, GPIO_QUAL_SYNC);

	GPIO_setPinConfig(myEPWM7_EPWMB_PIN_CONFIG);
	GPIO_setPadConfig(myEPWM7_EPWMB_GPIO, GPIO_PIN_TYPE_STD);
	GPIO_setQualificationMode(myEPWM7_EPWMB_GPIO, GPIO_QUAL_SYNC);

	// GPIO28 -> myGPIO28 Pinmux
	GPIO_setPinConfig(GPIO_28_GPIO28);
	// AGPIO -> GPIO mode selected
	GPIO_setAnalogMode(28, GPIO_ANALOG_DISABLED);

}

//*****************************************************************************
//
// ADC Configurations
//
//*****************************************************************************
void ADC_init(){
	myADC0_init();
	myADC1_init();
}

void myADC0_init(){
	//
	// ADC Initialization: Write ADC configurations and power up the ADC
	//
	// Set the analog voltage reference selection and ADC module's offset trims.
	// This function sets the analog voltage reference to internal (with the reference voltage of 1.65V or 2.5V) or external for ADC
	// which is same as ASysCtl APIs.
	//
	ADC_setVREF(myADC0_BASE, ADC_REFERENCE_INTERNAL, ADC_REFERENCE_3_3V);
	//
	// Configures the analog-to-digital converter module prescaler.
	//
	ADC_setPrescaler(myADC0_BASE, ADC_CLK_DIV_2_0);
	//
	// Sets the timing of the end-of-conversion pulse
	//
	ADC_setInterruptPulseMode(myADC0_BASE, ADC_PULSE_END_OF_CONV);
	//
	// Powers up the analog-to-digital converter core.
	//
	ADC_enableConverter(myADC0_BASE);
	//
	// Delay for 1ms to allow ADC time to power up
	//
	DEVICE_DELAY_US(5000);
	//
	// Enable alternate timings for DMA trigger
	//
	ADC_enableAltDMATiming(myADC0_BASE);
	//
	// SOC Configuration: Setup ADC EPWM channel and trigger settings
	//
	// Disables SOC burst mode.
	//
	ADC_disableBurstMode(myADC0_BASE);
	//
	// Sets the priority mode of the SOCs.
	//
	ADC_setSOCPriority(myADC0_BASE, ADC_PRI_ALL_ROUND_ROBIN);
	//
	// Start of Conversion 0 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 0
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN6
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN6, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER0, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 1 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 1
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN2
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN2, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER1, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 2 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 2
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN11
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER2, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN11, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER2, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 3 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 3
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN12
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER3, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN12, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER3, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 4 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 4
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN4
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER4, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN4, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER4, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 5 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 5
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN15
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER5, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN15, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER5, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 6 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 6
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN5
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER6, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN5, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER6, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 7 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 7
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN7
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER7, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN7, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER7, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 8 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 8
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN9
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER8, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN9, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER8, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 9 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 9
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN3
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER9, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN3, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER9, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 10 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 10
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN14
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER10, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN14, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER10, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 11 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 11
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN1
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER11, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN1, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER11, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 12 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 12
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN8
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER12, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN8, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER12, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 13 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 13
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN24
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER13, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN24, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER13, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 14 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 14
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN25
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC0_BASE, ADC_SOC_NUMBER14, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN25, 20U);
	ADC_setInterruptSOCTrigger(myADC0_BASE, ADC_SOC_NUMBER14, ADC_INT_SOC_TRIGGER_NONE);
	//
	// ADC Interrupt 1 Configuration
	// 		Source	: ADC_INT_TRIGGER_EOC0
	// 		Interrupt Source: enabled
	// 		Continuous Mode	: disabled
	//
	//
	ADC_setInterruptSource(myADC0_BASE, ADC_INT_NUMBER1, ADC_INT_TRIGGER_EOC0);
	ADC_clearInterruptStatus(myADC0_BASE, ADC_INT_NUMBER1);
	ADC_disableContinuousMode(myADC0_BASE, ADC_INT_NUMBER1);
	ADC_enableInterrupt(myADC0_BASE, ADC_INT_NUMBER1);
}

void myADC1_init(){
	//
	// ADC Initialization: Write ADC configurations and power up the ADC
	//
	// Set the analog voltage reference selection and ADC module's offset trims.
	// This function sets the analog voltage reference to internal (with the reference voltage of 1.65V or 2.5V) or external for ADC
	// which is same as ASysCtl APIs.
	//
	ADC_setVREF(myADC1_BASE, ADC_REFERENCE_INTERNAL, ADC_REFERENCE_3_3V);
	//
	// Configures the analog-to-digital converter module prescaler.
	//
	ADC_setPrescaler(myADC1_BASE, ADC_CLK_DIV_2_0);
	//
	// Sets the timing of the end-of-conversion pulse
	//
	ADC_setInterruptPulseMode(myADC1_BASE, ADC_PULSE_END_OF_ACQ_WIN);
	//
	// Sets the timing of early interrupt generation.
	//
	ADC_setInterruptCycleOffset(myADC1_BASE, 0U);
	//
	// Powers up the analog-to-digital converter core.
	//
	ADC_enableConverter(myADC1_BASE);
	//
	// Delay for 1ms to allow ADC time to power up
	//
	DEVICE_DELAY_US(5000);
	//
	// Enable alternate timings for DMA trigger
	//
	ADC_enableAltDMATiming(myADC1_BASE);
	//
	// SOC Configuration: Setup ADC EPWM channel and trigger settings
	//
	// Disables SOC burst mode.
	//
	ADC_disableBurstMode(myADC1_BASE);
	//
	// Sets the priority mode of the SOCs.
	//
	ADC_setSOCPriority(myADC1_BASE, ADC_PRI_ALL_ROUND_ROBIN);
	//
	// Start of Conversion 0 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 0
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN1
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC1_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN1, 20U);
	ADC_setInterruptSOCTrigger(myADC1_BASE, ADC_SOC_NUMBER0, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 1 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 1
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN12
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC1_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN12, 20U);
	ADC_setInterruptSOCTrigger(myADC1_BASE, ADC_SOC_NUMBER1, ADC_INT_SOC_TRIGGER_NONE);
	//
	// Start of Conversion 2 Configuration
	//
	//
	// Configures a start-of-conversion (SOC) in the ADC and its interrupt SOC trigger.
	// 	  	SOC number		: 2
	//	  	Trigger			: ADC_TRIGGER_EPWM7_SOCA
	//	  	Channel			: ADC_CH_ADCIN2
	//	 	Sample Window	: 20 SYSCLK cycles
	//		Interrupt Trigger: ADC_INT_SOC_TRIGGER_NONE
	//
	ADC_setupSOC(myADC1_BASE, ADC_SOC_NUMBER2, ADC_TRIGGER_EPWM7_SOCA, ADC_CH_ADCIN2, 20U);
	ADC_setInterruptSOCTrigger(myADC1_BASE, ADC_SOC_NUMBER2, ADC_INT_SOC_TRIGGER_NONE);
}


//*****************************************************************************
//
// ASYSCTL Configurations
//
//*****************************************************************************
void ASYSCTL_init(){
	//
	// asysctl initialization
	//
	// Disables the temperature sensor output to the ADC.
	//
	ASysCtl_disableTemperatureSensor();
	//
	// Set the analog voltage reference selection to internal.
	//
	ASysCtl_setAnalogReferenceInternal( ASYSCTL_ANAREF_INTREF_ADCA | ASYSCTL_ANAREF_INTREF_ADCB | ASYSCTL_ANAREF_INTREF_ADCC | ASYSCTL_ANAREF_INTREF_ADCD | ASYSCTL_ANAREF_INTREF_ADCE );

	//
	// Set the internal analog voltage reference selection to 1.65V.
	//
	ASysCtl_setAnalogReference1P65( ASYSCTL_ANAREF_ADCA | ASYSCTL_ANAREF_ADCB | ASYSCTL_ANAREF_ADCC | ASYSCTL_ANAREF_ADCD | ASYSCTL_ANAREF_ADCE );
}

//*****************************************************************************
//
// CPUTIMER Configurations
//
//*****************************************************************************
void CPUTIMER_init(){
	myCPUTIMER0_init();
}

void myCPUTIMER0_init(){
	CPUTimer_setEmulationMode(myCPUTIMER0_BASE, CPUTIMER_EMULATIONMODE_STOPAFTERNEXTDECREMENT);
	CPUTimer_setPreScaler(myCPUTIMER0_BASE, 0U);
	CPUTimer_setPeriod(myCPUTIMER0_BASE, 23437U);
	CPUTimer_enableInterrupt(myCPUTIMER0_BASE);
	CPUTimer_stopTimer(myCPUTIMER0_BASE);

	CPUTimer_reloadTimerCounter(myCPUTIMER0_BASE);
	CPUTimer_startTimer(myCPUTIMER0_BASE);
}

//*****************************************************************************
//
// DAC Configurations
//
//*****************************************************************************
void DAC_init(){
	myDAC0_init();
}

void myDAC0_init(){
	//
	// Set DAC reference voltage.
	//
	DAC_setReferenceVoltage(myDAC0_BASE, DAC_REF_ADC_VREFHI);
	//
	// Set DAC gain mode.
	//
	DAC_setGainMode(myDAC0_BASE, DAC_GAIN_TWO);
	//
	// Set DAC load mode.
	//
	DAC_setLoadMode(myDAC0_BASE, DAC_LOAD_SYSCLK);
	//
	// Enable the DAC output
	//
	DAC_enableOutput(myDAC0_BASE);
	//
	// Set the DAC shadow output
	//
	DAC_setShadowValue(myDAC0_BASE, 2048U);

	//
	// Delay for buffered DAC to power up.
	//
	DEVICE_DELAY_US(5000);
}

//*****************************************************************************
//
// DMA Configurations
//
//*****************************************************************************
void DMA_init(){
    DMA_initController();
	myDMA0_init();
}

void myDMA0_init(){
    DMA_setEmulationMode(DMA_EMULATION_FREE_RUN);
    DMA_configAddresses(myDMA0_BASE, (const void *)0, (const void *)2816);
    DMA_configBurst(myDMA0_BASE, 1U, 0, 1);
    DMA_configTransfer(myDMA0_BASE, 1028U, 0, 1);
    DMA_configWrap(myDMA0_BASE, 65535U, 0, 65535U, 0);
    DMA_configMode(myDMA0_BASE, DMA_TRIGGER_ADCA1, DMA_CFG_ONESHOT_DISABLE | DMA_CFG_CONTINUOUS_DISABLE | DMA_CFG_SIZE_16BIT);
    DMA_enableTrigger(myDMA0_BASE);
    DMA_stopChannel(myDMA0_BASE);
}

//*****************************************************************************
//
// EPWM Configurations
//
//*****************************************************************************
void EPWM_init(){
    EPWM_setClockPrescaler(myEPWM7_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);	
    EPWM_setTimeBasePeriod(myEPWM7_BASE, 23438);	
    EPWM_setTimeBaseCounter(myEPWM7_BASE, 0);	
    EPWM_setTimeBaseCounterMode(myEPWM7_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);	
    EPWM_enablePhaseShiftLoad(myEPWM7_BASE);	
    EPWM_setPhaseShift(myEPWM7_BASE, 300);	
    EPWM_setSyncInPulseSource(myEPWM7_BASE, EPWM_SYNC_IN_PULSE_SRC_INPUTXBAR_OUT5);	
    EPWM_setCounterCompareValue(myEPWM7_BASE, EPWM_COUNTER_COMPARE_A, 11719);	
    EPWM_setCounterCompareShadowLoadMode(myEPWM7_BASE, EPWM_COUNTER_COMPARE_A, EPWM_COMP_LOAD_ON_CNTR_ZERO);	
    EPWM_setCounterCompareValue(myEPWM7_BASE, EPWM_COUNTER_COMPARE_B, 0);	
    EPWM_setCounterCompareShadowLoadMode(myEPWM7_BASE, EPWM_COUNTER_COMPARE_B, EPWM_COMP_LOAD_ON_CNTR_ZERO);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_A, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPB);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_HIGH, EPWM_AQ_OUTPUT_ON_TIMEBASE_ZERO);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_PERIOD);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_LOW, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPA);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPA);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_UP_CMPB);	
    EPWM_setActionQualifierAction(myEPWM7_BASE, EPWM_AQ_OUTPUT_B, EPWM_AQ_OUTPUT_NO_CHANGE, EPWM_AQ_OUTPUT_ON_TIMEBASE_DOWN_CMPB);	
    EPWM_setRisingEdgeDelayCountShadowLoadMode(myEPWM7_BASE, EPWM_RED_LOAD_ON_CNTR_ZERO);	
    EPWM_disableRisingEdgeDelayCountShadowLoadMode(myEPWM7_BASE);	
    EPWM_setFallingEdgeDelayCountShadowLoadMode(myEPWM7_BASE, EPWM_FED_LOAD_ON_CNTR_ZERO);	
    EPWM_disableFallingEdgeDelayCountShadowLoadMode(myEPWM7_BASE);	
    EPWM_setTripZoneAction(myEPWM7_BASE, EPWM_TZ_ACTION_EVENT_TZA, EPWM_TZ_ACTION_LOW);	
    EPWM_setTripZoneAction(myEPWM7_BASE, EPWM_TZ_ACTION_EVENT_TZB, EPWM_TZ_ACTION_HIGH);	
    EPWM_enableTripZoneInterrupt(myEPWM7_BASE, EPWM_TZ_INTERRUPT_OST);	
    EPWM_enableADCTrigger(myEPWM7_BASE, EPWM_SOC_A);	
    EPWM_setADCTriggerSource(myEPWM7_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);	
    EPWM_setADCTriggerEventPrescale(myEPWM7_BASE, EPWM_SOC_A, 1);	
}

//*****************************************************************************
//
// GPIO Configurations
//
//*****************************************************************************
void GPIO_init(){
	myGPIO28_init();
}

void myGPIO28_init(){
	GPIO_setPadConfig(myGPIO28, GPIO_PIN_TYPE_STD);
	GPIO_setQualificationMode(myGPIO28, GPIO_QUAL_SYNC);
	GPIO_setDirectionMode(myGPIO28, GPIO_DIR_MODE_IN);
	GPIO_setControllerCore(myGPIO28, GPIO_CORE_CPU1);
}

//*****************************************************************************
//
// INPUTXBAR Configurations
//
//*****************************************************************************
void INPUTXBAR_init(){
	myINPUTXBARINPUT0_init();
}

void myINPUTXBARINPUT0_init(){
	XBAR_setInputPin(INPUTXBAR_BASE, myINPUTXBARINPUT0_INPUT, myINPUTXBARINPUT0_SOURCE);
}

//*****************************************************************************
//
// INTERRUPT Configurations
//
//*****************************************************************************
void INTERRUPT_init(){
	
	// Interrupt Settings for INT_myADC0_1
	// ISR need to be defined for the registered interrupts
	Interrupt_register(INT_myADC0_1, &adcA1ISR);
	Interrupt_enable(INT_myADC0_1);
	
	// Interrupt Settings for INT_myCPUTIMER0
	// ISR need to be defined for the registered interrupts
	Interrupt_register(INT_myCPUTIMER0, &cpuTimer0ISR);
	Interrupt_enable(INT_myCPUTIMER0);
	
	// Interrupt Settings for INT_myGPIO28_XINT
	// ISR need to be defined for the registered interrupts
	Interrupt_register(INT_myGPIO28_XINT, &GPIOISR);
	Interrupt_enable(INT_myGPIO28_XINT);
}
//*****************************************************************************
//
// SYNC Scheme Configurations
//
//*****************************************************************************
void SYNC_init(){
	SysCtl_setSyncOutputConfig(SYSCTL_SYNC_OUT_SRC_EPWM1SYNCOUT);
	//
	// SOCA
	//
	SysCtl_enableExtADCSOCSource(0);
	//
	// SOCB
	//
	SysCtl_enableExtADCSOCSource(0);
}
//*****************************************************************************
//
// XINT Configurations
//
//*****************************************************************************
void XINT_init(){
	myGPIO28_XINT_init();
}

void myGPIO28_XINT_init(){
	GPIO_setInterruptType(myGPIO28_XINT, GPIO_INT_TYPE_FALLING_EDGE);
	GPIO_setInterruptPin(myGPIO28, myGPIO28_XINT);
	GPIO_enableInterrupt(myGPIO28_XINT);
}

