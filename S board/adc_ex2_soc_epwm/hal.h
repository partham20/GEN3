/*
 * Copyright (c) 2024, Texas Instruments Incorporated
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

#ifndef HAL_H
#define HAL_H

#include "device.h"
// #include "f28p65x_device.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//*****************************************************************************
// Defines
//*****************************************************************************
#define TRUE    1
#define FALSE   0

// System clock frequency (adjust based on actual SYSCLK)
#define HAL_SYSTEM_FREQ_MHZ     (DEVICE_SYSCLK_FREQ / 1000000)

// Memory block addresses for F28P65x
#define FLASH_SECTOR_SIZE        0x1000  // 4KB sectors

//*****************************************************************************
// Enumerations
//*****************************************************************************
typedef enum {
    HAL_MEMORY_BLOCK_STARTADDR01 = 0x80000  // Adjust based on F28P65x memory map
} HAL_MEMORY_BLOCK_STARTADDR;

typedef enum {
    HAL_GPIO_PIN_LOW = 0,
    HAL_GPIO_PIN_HIGH
} HAL_GPIO_STATE;

typedef enum {
    HAL_GPIO_IN_00 = 0,
    HAL_GPIO_IN_01,
    HAL_GPIO_IN_02,
    HAL_GPIO_IN_03,
    HAL_GPIO_IN_04,
    HAL_GPIO_IN_MAX
} HAL_GPIO_IN;

typedef enum {
    HAL_GPIO_OUT_00 = 0,
    HAL_GPIO_OUT_01,
    HAL_GPIO_OUT_02,
    HAL_GPIO_OUT_03,
    HAL_GPIO_OUT_MAX
} HAL_GPIO_OUT;

//*****************************************************************************
// Structures
//*****************************************************************************
typedef struct HAL_GPIO_Instance_ {
    uint32_t            pin;        // GPIO number
    uint32_t            base;       // GPIO port base
    uint16_t            irqNum;     // GPIO IRQ number
} HAL_GPIOInstance;

//*****************************************************************************
// Global Variables
//*****************************************************************************
extern HAL_GPIOInstance gpioInputPin[HAL_GPIO_IN_MAX];
extern HAL_GPIOInstance gpioOutputPin[HAL_GPIO_OUT_MAX];


//*****************************************************************************
// Function Prototypes
//*****************************************************************************
static inline void HAL_delayMicroSeconds(uint32_t microSeconds) {
    DEVICE_DELAY_US(microSeconds);
}

// Function declarations - same as MSPM0Gxx0x HAL
void HAL_init(void);
bool HAL_readGPIOPin(HAL_GPIO_IN pin);
void HAL_writeGPIOPin(HAL_GPIO_OUT pin, HAL_GPIO_STATE pinState);
void HAL_enableGPIOInterrupt(HAL_GPIO_IN pin);
uint32_t HAL_getGPIOEnabledInterruptStatus(HAL_GPIO_IN pin);
void HAL_clearGPIOInterruptStatus(HAL_GPIO_IN pin);
void HAL_clearMemoryBlock(HAL_MEMORY_BLOCK_STARTADDR startAddr);
void HAL_copyMemoryBlock(void *dstAddr, void *srcAddr, int len, HAL_MEMORY_BLOCK_STARTADDR startAddr);

#ifdef __cplusplus
}
#endif
#endif /* HAL_H */ 
