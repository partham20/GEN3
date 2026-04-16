//###########################################################################
//
// FILE:   ex_4_cpu1bootrom.h
//
// TITLE:  BootROM Definitions.
//
//###########################################################################
// $TI Release:  $
// 
// $Copyright:
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

#ifndef C_BOOTROM_H_
#define C_BOOTROM_H_

#include <stdint.h>
#include "hw_types.h"
#include "hw_memmap.h"
#include "sysctl.h"
#include "cpu.h"
#include "hw_flash_command.h"
#include "hw_dcsm.h"
#include "pin_map.h"
#include "gpio.h"
#include "spi.h"
#include "dcc.h"
#include "ex_4_cpu1brom_boot_modes.h"
#include "hw_memcfg.h"
#include "ex_4_bootloader_mcan.h"

// //*************************************************************************************************
// // Clocking defines
// //
#define CLK_INTOSC2_FREQ_MHZ            10UL

#define CLK_INTOSC2                     0U
#define CLK_XTAL                        1U

#define APLL_MULT_46                    46UL
#define APLL_MULT_30                    30UL
#define APLL_MULT_24                    24UL
#define APLL_MULT_12                    12UL
#define APLL_MULT_5                     5UL
#define APLL_MULT_4                     4UL

#define APLL_DIV_4                      3UL
#define APLL_DIV_2                      1UL
#define APLL_DIV_1                      0UL

#define SYSCLK_DIV_1                    0UL
#define SYSCLK_DIV_2                    1UL
#define SYSCLK_DIV_4                    2UL



//*************************************************************************************************
// Function prototypes
//
 extern void CPU1BROM_triggerSysPLLLock(uint32_t clkSource, uint32_t multiplier, uint32_t divider);
 extern uint16_t CPU1BROM_switchToPLL(uint32_t pllInputClockMhz);


#endif //C_BOOTROM_H_
