/*
 * fw_update_flash.c
 *
 *  Created on: 02-Apr-2026
 *      Author: Parthasarathy.M
 */

#include "flash_driver.h"


/*
  What You Need to Do

  Goal: Erase Bank 2, write a test pattern to it, read it back and verify.

  What you already know: Your flash_driver.c does this exact thing for Bank 4 (calibration data). Open it and read it — that's your reference.

  Steps

  1. Read flash_driver.c — understand these 3 operations:
    - Fapi_setActiveFlashBank() — selects which bank to operate on
    - Fapi_issueBankEraseCommand() — erases a bank
    - Fapi_issueAutoEcc512ProgrammingCommand() or Fapi_issueProgrammingCommand() — writes data
  2. Create fw_update_flash.c with one function: FW_testBank2()
    - Set active bank
    - Erase Bank 2 (address 0x0C0000)
    - Write 32 words of test data (e.g., 0x1234) to 0x0C0000
    - Read it back and check if it matches
  3. Call FW_testBank2() from somewhere in main() (before the while loop, after startup())
  4. Build, load, set a breakpoint after the verify step, check if it passed

  Hints

  - Look at flash_programming_f28p55x.h in C2000Ware for Bank 2 sector addresses
  - Flash writes must be 512-bit aligned (32 words at a time)
  - The Flash API functions must run from RAM (.TI.ramfunc section) — your existing code already handles this
  - EALLOW before flash operations, EDIS after

  Start by reading flash_driver.c line by line. What do you see that you don't understand?
*/

void FW_testBank2(void);
void FW_eraseFlashRange(uint32_t bankBaseAddress, uint32_t sectorCount);
void FW_writeTestPattern(uint32_t bankBaseAddress, uint32_t sectorCount);

#pragma CODE_SECTION(FW_eraseFlashRange, ".TI.ramfunc");
void FW_eraseFlashRange(uint32_t bankBaseAddress, uint32_t sectorCount)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;
    uint32_t currentSectorAddress;
    uint32_t sectorIndex;

    for (sectorIndex = 0; sectorIndex < sectorCount; sectorIndex++)
    {
        currentSectorAddress = bankBaseAddress + (sectorIndex * SECTOR_SIZE);

        ClearFSMStatus();

        Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
        Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

        oReturnCheck = Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, (uint32_t *)currentSectorAddress);

        while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady)
        {
        }

        if (oReturnCheck != Fapi_Status_Success)
        {
            extern volatile uint32_t fwDbgEraseError, fwDbgEraseSector;
            fwDbgEraseError = (uint32_t)oReturnCheck;
            fwDbgEraseSector = sectorIndex;
            Example_Error(oReturnCheck);
            return;
        }

        oFlashStatus = Fapi_getFsmStatus();
        if (oFlashStatus != 3)
        {
            extern volatile uint32_t fwDbgFsmStatus, fwDbgEraseSector;
            fwDbgFsmStatus = (uint32_t)oFlashStatus;
            fwDbgEraseSector = sectorIndex;
            FMSTAT_Fail();
            return;
        }

        oReturnCheck = Fapi_doBlankCheck((uint32_t *)currentSectorAddress, Sector2KB_u32length, &oFlashStatusWord);
        if (oReturnCheck != Fapi_Status_Success)
        {
            extern volatile uint32_t fwDbgEraseError, fwDbgEraseSector;
            fwDbgEraseError = 100U + (uint32_t)oReturnCheck;  /* 100+ = blank check fail */
            fwDbgEraseSector = sectorIndex;
            Example_Error(oReturnCheck);
            return;
        }
    }
}

/* Writes a repeating 0xABCD test pattern across sectorCount sectors starting
 * at bankBaseAddress. Flash writes are 512-bit (8 x uint16) aligned chunks. */
#pragma CODE_SECTION(FW_writeTestPattern, ".TI.ramfunc");
void FW_writeTestPattern(uint32_t bankBaseAddress, uint32_t sectorCount)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;
    uint32_t flashWordAddress;
    uint32_t totalWords;       /* total 16-bit words in the range */
    uint16_t patternBuf[8];
    uint16_t wordIdx;

    /* Fill the 8-word pattern buffer once — same value reused every chunk */
    for (wordIdx = 0; wordIdx < 8; wordIdx++)
    {
        patternBuf[wordIdx] = 0xABCDU;
    }

    /* Sector2KB_u16length (0x400) = 16-bit words per sector */
    totalWords = sectorCount * Sector2KB_u16length;

    for (flashWordAddress = bankBaseAddress;
         flashWordAddress < (bankBaseAddress + totalWords);
         flashWordAddress += 8U)
    {
        ClearFSMStatus();

        Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
        Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

        oReturnCheck = Fapi_issueProgrammingCommand((uint32_t *)flashWordAddress,
                                                    patternBuf, 8,
                                                    0, 0,
                                                    Fapi_AutoEccGeneration);

        while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy)
        {
        }

        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
            return;
        }

        oFlashStatus = Fapi_getFsmStatus();
        if (oFlashStatus != 3)
        {
            FMSTAT_Fail();
            return;
        }

        oReturnCheck = Fapi_doVerify((uint32_t *)flashWordAddress, 4,
                                     (uint32_t *)(uint32_t)patternBuf,
                                     &oFlashStatusWord);
        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
            return;
        }
    }
}

void FW_testBank2(void)
{
    FW_eraseFlashRange(0x0C0000U, 128);
    FW_writeTestPattern(0x0C0000U, 128);
}
