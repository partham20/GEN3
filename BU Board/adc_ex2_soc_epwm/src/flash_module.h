/*
 * flash_module.h
 *
 * F021 Flash API Operations for Bank 4 Calibration Storage
 *
 * Provides flash erase, program, and sector management for
 * storing calibration data in Bank 4 (32 sectors x 1KB)
 *
 * Note: Flash API functions marked with .TI.ramfunc run from RAM
 *       since flash cannot be read while being programmed
 *
 *  Created on: Feb 19, 2026
 *      Author: Parthasarathy.M
 */

#ifndef SRC_FLASH_MODULE_H_
#define SRC_FLASH_MODULE_H_

#include "can_bu.h"

#ifdef CAN_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

#include "flash_config.h"
#include "pdu_manager.h"

/* ========================================================================== */
/*                       Flash Initialization                                 */
/* ========================================================================== */

/* Initialize F021 Flash API (runs from RAM) */
int FlashAPI_init(void);

/* ========================================================================== */
/*                       Flash Operations                                     */
/* ========================================================================== */

/* Write data from Buffer[] to flash at addressToWrite */
void writeToFlash(void);

/* Erase a single sector at addressToWrite */
void Example_EraseSector(void);

/* Erase entire Bank 4 */
void eraseBank4(void);

/* Prepare and write default calibration values */
void writeDefaultCalibrationValues(void);

/* ========================================================================== */
/*                       Flash Status                                         */
/* ========================================================================== */

/* Clear Flash State Machine status */
void ClearFSMStatus(void);

/* Error/status handlers */
void Example_Error(Fapi_StatusType status);
void Example_Done(void);
void FMSTAT_Fail(void);

/* ========================================================================== */
/*                       Sector Management                                    */
/* ========================================================================== */

/* Scan Bank 4 to find newest/oldest data and read/write addresses */
void findReadAndWriteAddress(void);

/* Find empty sector or erase oldest for writing */
void eraseSectorOrFindEmptySector(void);

/* Find first empty sector in Bank 4 */
uint32_t findEmptySector(void);

/* Convert flash address to sector number */
uint16_t addressToSector(uint32_t addr);

/* ========================================================================== */
/*                       High-Level Init                                      */
/* ========================================================================== */

/**
 * @brief   Initialize flash and load calibration data
 *          - Inits Flash API
 *          - Scans Bank 4 for existing data
 *          - If no data found, writes defaults
 *          - Loads calibration into pduManager.workingData
 * @return  true on success, false on flash error
 */
bool initializeFlashAndLoadData(void);

#ifdef __cplusplus
}
#endif

#endif /* CAN_ENABLED */

#endif /* SRC_FLASH_MODULE_H_ */
