/*
 * flash_config.h
 *
 * Flash and Memory Constants for Bank 4 Calibration Storage
 * Adapted from reference project config.h
 *
 * Bank 4: 32 sectors x 1KB = 32KB (0x100000 - 0x107FFF)
 * Sector wrapping with WRAP_LIMIT for flash wear leveling
 *
 *  Created on: Feb 19, 2026
 *      Author: Parthasarathy.M
 */

#ifndef SRC_FLASH_CONFIG_H_
#define SRC_FLASH_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "driverlib.h"
#include "device.h"

/* Flash API headers */
#include "FlashTech_F28P55x_C28x.h"
#include "flash_programming_f28p55x.h"

/* ========================================================================== */
/*                         Flash Memory Constants                             */
/* ========================================================================== */

/* Bank 4 address range */
#define BANK4_START                     0x100000U
#define BANK4_END                       FlashBank4EndAddress    /* 0x107FFF */
#define SECTOR_SIZE                     0x400U                  /* 1KB per sector */

/* Sector index wrap limit for wear leveling */
extern uint16_t WRAP_LIMIT  ;

/* Flash write buffer size (words) */
#define WORDS_IN_FLASH_BUFFER           0x400

/* Flash Sector Write Enable Codes */
#define SEC0TO31                        0x00000000U
#define SEC32To127                      0xFFFFFFFFU

/* Number of branches (CTs) for calibration data */
#define FLASH_NUM_BRANCHES              18U

/* Default calibration gain values (uint16_t, stored in flash) */
/* 1000 = 1.0x gain (divide by CAL_GAIN_SCALE at runtime) */
#define DEFAULT_CURRENT_GAIN            10000U
#define DEFAULT_KW_GAIN                 10000U

#endif /* SRC_FLASH_CONFIG_H_ */
