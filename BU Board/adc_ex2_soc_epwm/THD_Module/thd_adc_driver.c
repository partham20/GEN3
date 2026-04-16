//#############################################################################
//! \file thd_adc_driver.c
//! \brief F28P55x ADC/EPWM hardware driver implementation
//#############################################################################

#include "thd_adc_driver.h"

// Global state instance (accessible by ISR)
THD_ADC_State g_adcState;

// ---- ADC Initialization ----
void THD_ADC_init(void)
{
    g_adcState.sampleIndex     = 0;
    g_adcState.bufferFull      = 0;
    g_adcState.conversionCount = 0;

    ADC_setPrescaler(ADCA_BASE, ADC_CLK_DIV_4_0);
//    ADC_setVREF(ADCA_BASE, ADC_REFERENCE_INTERNAL, ADC_REFERENCE_3_3V);
//    ADC_setInterruptPulseMode(ADCA_BASE, ADC_PULSE_END_OF_CONV);
//    ADC_enableConverter(ADCA_BASE);
//    DEVICE_DELAY_US(1000);
//
//    // Configure SOCs for 5 channels
//    // Ch0=ADCIN2, Ch1=ADCIN6, Ch2=ADCIN15, Ch3=ADCIN14, Ch4=ADCIN3
//    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER0, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN15, 15);
//    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER1, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN15, 15);
//    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER2, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN15, 15);
//    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER3, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN15, 15);
//    ADC_setupSOC(ADCA_BASE, ADC_SOC_NUMBER4, ADC_TRIGGER_EPWM1_SOCA, ADC_CH_ADCIN15, 15);
//
//    // Interrupt on last SOC
//    ADC_setInterruptSource(ADCA_BASE, ADC_INT_NUMBER1, ADC_SOC_NUMBER4);
//    ADC_enableInterrupt(ADCA_BASE, ADC_INT_NUMBER1);
//    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
}

// ---- EPWM Initialization ----
void THD_ADC_initEPWM(void)
{
    SysCtl_disablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
    EPWM_setTimeBaseCounter(EPWM1_BASE, 0U);
    EPWM_setTimeBasePeriod(EPWM1_BASE, THD_EPWM_TIMER_TBPRD);
    EPWM_setPhaseShift(EPWM1_BASE, 0U);
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_STOP_FREEZE);
    EPWM_setClockPrescaler(EPWM1_BASE, EPWM_CLOCK_DIVIDER_1, EPWM_HSCLOCK_DIVIDER_1);
    EPWM_setADCTriggerSource(EPWM1_BASE, EPWM_SOC_A, EPWM_SOC_TBCTR_ZERO);
    EPWM_setADCTriggerEventPrescale(EPWM1_BASE, EPWM_SOC_A, 1U);
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
    SysCtl_enablePeripheral(SYSCTL_PERIPH_CLK_TBCLKSYNC);
}

// ---- Start Acquisition ----
void THD_ADC_start(void)
{
    Interrupt_register(INT_ADCA1, &THD_ADC_isr);
    Interrupt_enable(INT_ADCA1);
    EINT;
    ERTM;
    EPWM_enableADCTrigger(EPWM1_BASE, EPWM_SOC_A);
    EPWM_setTimeBaseCounterMode(EPWM1_BASE, EPWM_COUNTER_MODE_UP);
}

// ---- Reset and Rearm ----
void THD_ADC_resetAndRearm(void)
{
    g_adcState.bufferFull  = 0;
    g_adcState.sampleIndex = 0;
  //  Interrupt_enable(INT_ADCA1);//doubt
}

// ---- Buffer Status ----
int THD_ADC_isBufferReady(void)
{
    return (g_adcState.bufferFull != 0) ? 1 : 0;
}

// ---- Get Channel Buffer ----
const float32_t* THD_ADC_getChannelBuffer(int channel)
{
    if(channel < 0 || channel >= THD_NUM_CHANNELS) return (void*)0;
    return g_adcState.rawBuffers[channel];
}

// ---- ADC ISR ----
__interrupt void THD_ADC_isr(void)
{
//    uint16_t resA2  = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER0);
//    uint16_t resA6  = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER1);
//    uint16_t resA15 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER2);
//    uint16_t resA14 = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER3);
//    uint16_t resA3  = ADC_readResult(ADCARESULT_BASE, ADC_SOC_NUMBER4);
//
//    g_adcState.conversionCount++;
//
//    if(g_adcState.sampleIndex < THD_FFT_SIZE)
//    {
//        uint16_t idx = g_adcState.sampleIndex;
//
//        g_adcState.rawBuffers[0][idx] = (float32_t)resA2  * THD_ADC_SCALE;
//        g_adcState.rawBuffers[1][idx] = (float32_t)resA6  * THD_ADC_SCALE;
//        g_adcState.rawBuffers[2][idx] = (float32_t)resA15 * THD_ADC_SCALE;
//        g_adcState.rawBuffers[3][idx] = (float32_t)resA14 * THD_ADC_SCALE;
//        g_adcState.rawBuffers[4][idx] = (float32_t)resA3  * THD_ADC_SCALE;
//
//        g_adcState.sampleIndex++;
//    }
//
//    if(g_adcState.sampleIndex >= THD_FFT_SIZE)
//    {
//        g_adcState.bufferFull = 1;
//        Interrupt_disable(INT_ADCA1);
//    }
//
//    ADC_clearInterruptStatus(ADCA_BASE, ADC_INT_NUMBER1);
//    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP1);
}
