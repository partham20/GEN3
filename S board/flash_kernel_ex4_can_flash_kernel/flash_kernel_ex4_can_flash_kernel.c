//###########################################################################
//
// FILE:    flash_kernel_ex4_can_flash_kernel.c
//
// TITLE:   MCAN Flash Kernel Example for F28P55x
//
//###########################################################################
//
// C2000Ware v6.00.01.00
//
// Copyright (C) 2024 Texas Instruments Incorporated - http://www.ti.com
//
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions 
// are met:
// 
//   Redistributions of source code must retain the above copyright 
//   notice, this list of conditions and the following disclaimer.
// 
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the 
//   documentation and/or other materials provided with the   
//   distribution.
// 
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $

//###########################################################################

//
// Included Files
//
#include "ex_4_cpu1bootrom.h"
#include "device.h"
#include "gpio.h"
#include "driverlib.h"

//
// Globals
//
// Number of flash banks to erase before programming the application to Flash
#define NUM_FLASH_BANKS  5

extern uint32_t MCAN_Boot(uint32_t bootMode, uint32_t nominalBitTiming,
                   uint32_t dataBitTiming, uint16_t deviceSystemFreq,
                   uint16_t mcanDivider, uint16_t clockSelection,
                   uint16_t Num_Flash_Banks, uint32_t* Flash_Banks_To_Erase, uint32_t* CMD_WE_Protection_A_Masks,
                   uint32_t* CMD_WE_Protection_B_Masks, uint32_t* CMD_WE_Protection_UO_Masks);
//
// Function Prototypes
//
void (*ApplicationPtr) (void);

//
// main
//
uint32_t main(void)
{

    //
    // Initialize device clock and peripherals
    //
    Device_init();

    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, DEVICE_FLASH_WAITSTATES);

    //
    // Initialize GPIO
    //
    Device_initGPIO();

    //
    // Initialize MCAN and wait to recieve messages
    //
    uint32_t entryAddr;

    // Initialize configurable arrays that specify which flash banks to erase and appropriate protection masks when programming application
    uint32_t Flash_Banks_To_Erase[NUM_FLASH_BANKS] = {0,1,2,3,4};
    uint32_t CMD_WE_Protection_A_Masks[NUM_FLASH_BANKS] = {0,0,0,0,0};
    uint32_t CMD_WE_Protection_B_Masks[NUM_FLASH_BANKS] = {0,0,0,0,0};
    uint32_t CMD_WE_Protection_UO_Masks[NUM_FLASH_BANKS] = {0,0,0,0,0};

    // Get entry address of application
    entryAddr = MCAN_Boot(MCAN_BOOT_SENDTEST, 0, 0, 20, 4, 1, NUM_FLASH_BANKS, Flash_Banks_To_Erase,
                          CMD_WE_Protection_A_Masks, CMD_WE_Protection_B_Masks, CMD_WE_Protection_UO_Masks);

    // branch to application entrypoint
    asm(" MOVL  XAR7, ACC ");
    asm(" LB *XAR7 ");

    return(entryAddr);

}

//
// End of File
//

