/**
 * @file flash_driver.h
 * @brief Flash memory operations
 * sBoardRuntimeData
 */

#ifndef FLASH_DRIVER_H
#define FLASH_DRIVER_H

#include "common.h"
#include "FlashTech_F28P55x_C28x.h"
#include "flash_programming_f28p55x.h"

// Flash operation function prototypes
int FlashAPI_init(void);
void ClearFSMStatus(void);
void Example_ProgramUsingAutoECC(void);
void Example_EraseSector(void);
void eraseBank4(void);

// Firmware update flash operations (fw_update_flash.c)
void FW_eraseFlashRange(uint32_t bankBaseAddress, uint32_t sectorCount);
void FW_writeTestPattern(uint32_t bankBaseAddress, uint32_t sectorCount);
void FW_testBank2(void);
void Example_Error(Fapi_StatusType status);
void Example_Done(void);
void FMSTAT_Fail(void);
void ECC_Fail(void);

// Flash management function prototypes
void findReadAndWriteAddress(void);
void eraseSectorOrFindEmptySector(void);
int enableFlashWrite(void);

// Acknowledgement function prototypes
void calibDataUpdateSuccessAck(void);
void calibDataUpdateFailAck(void);
void flashWriteEnabledAck(void);
void flashWriteDisabledAck(void);
void erasedBank4Ack(void);
void writtenDefaultCalibrationValuesAck(void);

// External variable declarations
extern uint32_t addressToWrite;
extern uint16_t newestData;
extern uint16_t oldestData;
extern uint32_t writeAddress;
extern uint32_t readAddress;
extern bool emptySectorFound;
extern uint16_t Buffer[WORDS_IN_FLASH_BUFFER];
extern uint32_t *Buffer32;
extern size_t datasize;
extern bool updateFailed;
extern bool flashWriteEnabled;

#endif // FLASH_DRIVER_H
