//#############################################################################
//! \file thd_adc_driver.h
//! \brief F28P55x ADC/EPWM hardware driver for THD sampling
//!
//! Platform-specific ADC acquisition layer. Handles:
//!   - ADC peripheral initialization
//!   - EPWM timer configuration for sample triggering
//!   - ISR-based multi-channel data collection
//!   - Buffer management with double-buffering flags
//!
//! This module is F28P55x-specific. Replace it when porting to other
//! platforms while keeping thd_analyzer.h and thd_signal_processing.h.
//#############################################################################

#ifndef THD_ADC_DRIVER_H
#define THD_ADC_DRIVER_H

#include "thd_config.h"
#include "driverlib.h"
#include "device.h"

// ---- ADC Driver State ----
typedef struct {
    // Multi-channel raw data storage
    // Laid out as [channel][sample] for easy memcpy per channel
    float32_t   rawBuffers[THD_NUM_CHANNELS][THD_FFT_SIZE];

    // Acquisition state
    volatile uint16_t sampleIndex;
    volatile uint16_t bufferFull;
    volatile uint32_t conversionCount;
} THD_ADC_State;

// ---- Global ADC state (needed by ISR) ----
extern THD_ADC_State g_adcState;

// ---- API Functions ----

//! Initialize ADC peripheral (prescaler, VREF, SOCs, interrupts)
void THD_ADC_init(void);

//! Initialize EPWM1 for ADC trigger at the configured sampling frequency
void THD_ADC_initEPWM(void);

//! Start ADC acquisition (enable interrupt, start EPWM counting)
void THD_ADC_start(void);

//! Reset acquisition state and re-enable for the next frame
void THD_ADC_resetAndRearm(void);

//! Check if a full frame of samples has been collected
//! \return 1 if buffer is full and ready for processing, 0 otherwise
int THD_ADC_isBufferReady(void);

//! Get pointer to the raw buffer for a specific channel
//! \param[in]  channel  Channel index (0 to THD_NUM_CHANNELS-1)
//! \return     Pointer to float32_t[THD_FFT_SIZE] samples, or NULL if invalid
const float32_t* THD_ADC_getChannelBuffer(int channel);

//! ADC ISR handler - must be registered with Interrupt_register()
__interrupt void THD_ADC_isr(void);

#endif // THD_ADC_DRIVER_H
