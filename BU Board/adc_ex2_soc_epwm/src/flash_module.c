/*
 * flash_module.c
 *
 * F021 Flash API Operations for Bank 4 Calibration Storage
 *
 * Adapted from reference project flash_module.c
 * Provides flash erase, program, and sector management for
 * storing calibration data in Bank 4 (32 sectors x 1KB)
 *
 * Functions marked with .TI.ramfunc execute from RAM since
 * flash cannot be read while the Flash State Machine is active.
 *
 *  Created on: Feb 19, 2026
 *      Author: Parthasarathy.M
 */

#include "can_bu.h"

#ifdef CAN_ENABLED

#include "flash_module.h"

uint16_t WRAP_LIMIT   = 0xFFFE;

/* ========================================================================== */
/*                       Flash API Initialization                             */
/* ========================================================================== */

#pragma CODE_SECTION(FlashAPI_init, ".TI.ramfunc");
int FlashAPI_init(void)
{
    Fapi_StatusType oReturnCheck;

    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, 3);

    oReturnCheck = Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                                       DEVICE_SYSCLK_FREQ / 1000000U);
    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
        return -1;
    }

    oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank0);
    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
        return -1;
    }

    return 0;
}

/* ========================================================================== */
/*                       Flash Status Management                              */
/* ========================================================================== */

#pragma CODE_SECTION(ClearFSMStatus, ".TI.ramfunc");
void ClearFSMStatus(void)
{
    Fapi_FlashStatusType oFlashStatus;
    Fapi_StatusType oReturnCheck;

    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}

    oFlashStatus = Fapi_getFsmStatus();
    if (oFlashStatus != 0)
    {
        oReturnCheck = Fapi_issueAsyncCommand(Fapi_ClearStatus);

        while (Fapi_getFsmStatus() != 0) {}

        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
        }
    }
}

void Example_Error(Fapi_StatusType status)
{
    updateFailed = true;
}

void Example_Done(void)
{
    updateFailed = false;
}

void FMSTAT_Fail(void)
{
    updateFailed = true;
}

/* ========================================================================== */
/*                       Flash Write Operations                               */
/* ========================================================================== */

#pragma CODE_SECTION(writeToFlash, ".TI.ramfunc");
void writeToFlash(void)
{
    uint32_t u32Index = 0;
    uint16_t i = 0;
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;

    for (i = 0, u32Index = addressToWrite;
         (u32Index < (addressToWrite + datasize));
         i += 8, u32Index += 8)
    {
        ClearFSMStatus();

        Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA,
                                    SEC0TO31);
        Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB,
                                    SEC32To127);

        oReturnCheck = Fapi_issueProgrammingCommand((uint32_t *)u32Index,
                                                     Buffer + i,
                                                     8, 0, 0,
                                                     Fapi_AutoEccGeneration);

        while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy);

        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
        }

        oFlashStatus = Fapi_getFsmStatus();
        if (oFlashStatus != 3)
        {
            FMSTAT_Fail();
        }

        oReturnCheck = Fapi_doVerify((uint32_t *)u32Index,
                                      4, (uint32_t *)(uint32_t)(Buffer + i),
                                      &oFlashStatusWord);

        Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA,
                                    0xFFFFFFFF);

        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
        }
        if (oReturnCheck == Fapi_Status_Success)
        {
            Example_Done();
        }
    }
}

/* ========================================================================== */
/*                       Flash Erase Operations                               */
/* ========================================================================== */

#pragma CODE_SECTION(Example_EraseSector, ".TI.ramfunc");
void Example_EraseSector(void)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;

    ClearFSMStatus();

    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA,
                                SEC0TO31);
    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB,
                                SEC32To127);

    oReturnCheck = Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector,
                                                      (uint32_t *)addressToWrite);

    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}

    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }

    oFlashStatus = Fapi_getFsmStatus();
    if (oFlashStatus != 3)
    {
        FMSTAT_Fail();
    }

    oReturnCheck = Fapi_doBlankCheck((uint32_t *)addressToWrite,
                                      Sector2KB_u32length,
                                      &oFlashStatusWord);
    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }
}

#pragma CODE_SECTION(eraseBank4, ".TI.ramfunc");
void eraseBank4(void)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;
    uint32_t u32CurrentAddress = 0;

    ClearFSMStatus();

    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA,
                                0x00000000);
    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB,
                                0xFFFFFFFF);

    u32CurrentAddress = FlashBank4StartAddress;

    oReturnCheck = Fapi_issueBankEraseCommand((uint32_t *)u32CurrentAddress);

    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}

    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }

    oFlashStatus = Fapi_getFsmStatus();
    if (oFlashStatus != 3)
    {
        FMSTAT_Fail();
    }

    /* Blank check first 8 sectors */
    u32CurrentAddress = FlashBank4StartAddress;
    oReturnCheck = Fapi_doBlankCheck((uint32_t *)u32CurrentAddress,
                                      (8 * Sector2KB_u32length),
                                      &oFlashStatusWord);
    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }

    /* Blank check remaining sectors */
    u32CurrentAddress = FlashBank4StartAddress + 0xC000;
    oReturnCheck = Fapi_doBlankCheck((uint32_t *)u32CurrentAddress,
                                      (80 * Sector2KB_u32length),
                                      &oFlashStatusWord);
    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }
}

/* ========================================================================== */
/*                    Default Calibration Values                              */
/* ========================================================================== */

void writeDefaultCalibrationValues(void)
{
    int i;

    /* Initialize default calibration data */
    CalibData defaultPduData;
    memset(&defaultPduData, 0, sizeof(CalibData));

    defaultPduData.sectorIndex = ++newestData;
    defaultPduData.BUBoardID = address;

    for (i = 0; i < FLASH_NUM_BRANCHES; i++)
    {
        defaultPduData.currentGain[i] = DEFAULT_CURRENT_GAIN;
        defaultPduData.kwGain[i] = DEFAULT_KW_GAIN;
    }

    /* Copy to readWriteData and Buffer for flash writing */
    memcpy(&pduManager.readWriteData, &defaultPduData, sizeof(CalibData));
    memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibData));
    datasize = sizeof(CalibData) / sizeof(uint16_t);

    pduManager.dataSync = false;
}

/* ========================================================================== */
/*                       Sector Management                                    */
/* ========================================================================== */

void findReadAndWriteAddress(void)
{
    uint32_t addr;
    uint16_t val;

    newestData = 0;
    oldestData = 0xFFFF;
    writeAddress = 0xFFFFFFFF;
    readAddress = BANK4_START;
    emptySectorFound = false;

    for (addr = BANK4_START; addr <= BANK4_END; addr += SECTOR_SIZE)
    {
        val = *(volatile uint16_t *)addr;

        if (val == 0xFFFF)
        {
            emptySectorFound = true;
            continue;
        }

        /* Track newest (highest sectorIndex) */
        if (val <= WRAP_LIMIT && val > newestData)
        {
            newestData = val;
            readAddress = addr;
        }

        /* Track oldest (lowest sectorIndex) */
        if (val <= WRAP_LIMIT && val < oldestData && val > 0)
        {
            oldestData = val;
            writeAddress = addr;
        }
    }
}

#pragma CODE_SECTION(eraseSectorOrFindEmptySector, ".TI.ramfunc");
void eraseSectorOrFindEmptySector(void)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusWordType oFlashStatusWord;
    uint32_t i;

    /* Try to find an empty sector first */
    if (emptySectorFound)
    {
        for (i = BANK4_START; i <= BANK4_END; i += SECTOR_SIZE)
        {
            oReturnCheck = Fapi_doBlankCheck((uint32_t *)i,
                                              Sector2KB_u32length,
                                              &oFlashStatusWord);
            if (oReturnCheck == Fapi_Status_Success)
            {
                addressToWrite = i;
                return;
            }
        }
    }

    /* No empty sector - erase oldest */
    if ((oldestData < WRAP_LIMIT) && (newestData != WRAP_LIMIT))
    {
        addressToWrite = writeAddress;
        Example_EraseSector();
        return;
    }
}

uint32_t findEmptySector(void)
{
    uint32_t addr;
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusWordType oFlashStatusWord;

    for (addr = BANK4_START; addr <= BANK4_END; addr += SECTOR_SIZE)
    {
        oReturnCheck = Fapi_doBlankCheck((uint32_t *)addr,
                                          Sector2KB_u32length,
                                          &oFlashStatusWord);
        if (oReturnCheck == Fapi_Status_Success)
        {
            return addr;
        }
    }

    return 0xFFFFFFFF;
}

uint16_t addressToSector(uint32_t addr)
{
    if (addr < BANK4_START || addr > BANK4_END)
    {
        return 0xFFFF;
    }

    return (uint16_t)((addr - BANK4_START) / SECTOR_SIZE);
}

/* ========================================================================== */
/*                    High-Level Initialization                               */
/* ========================================================================== */

bool initializeFlashAndLoadData(void)
{
    /* Initialize Flash API */
    FlashAPI_init();

    /* Initialize PDU manager */
    initializePDUManager();

    /* Find read and write addresses in Bank 4 */
    findReadAndWriteAddress();

    /* If no calibration data exists, write defaults */
    if (newestData == 0)
    {
        addressToWrite = BANK4_START;
        writeDefaultCalibrationValues();
        writeToFlash();

        if (!updateFailed)
        {
            syncReadWriteToWorking();
            readAddress = BANK4_START;
            return true;
        }
        else
        {
            return false;
        }
    }
    else
    {
        /* Load existing data from flash */
        memcpy(&pduManager.readWriteData, (void *)readAddress, sizeof(CalibData));
        syncReadWriteToWorking();
        return true;
    }
}

#endif /* CAN_ENABLED */
