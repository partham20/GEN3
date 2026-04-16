/**
 * @file flash_driver.c
 * @brief Flash memory operations implementation
 */

#include "flash_driver.h"
#include "m_board.h"
#include "can_driver.h"

// Global variables
uint32_t addressToWrite = BANK4_START;
uint16_t newestData = 0;
uint16_t oldestData = 0xFFFF;
uint32_t writeAddress = 0xFFFFFFFF;
uint32_t readAddress = BANK4_START;
bool emptySectorFound = false;
uint16_t Buffer[WORDS_IN_FLASH_BUFFER];
uint32_t *Buffer32 = (uint32_t *)Buffer;
size_t datasize;
bool updateFailed = false;
bool flashWriteEnabled = false;

/**
 * @brief Initialize Flash API
 * @return int Status code
 */
#pragma CODE_SECTION(FlashAPI_init, ".TI.ramfunc");
int FlashAPI_init(void)
{
    Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, 3);

    Fapi_StatusType oReturnCheck;

    oReturnCheck = Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                                      DEVICE_SYSCLK_FREQ / 1000000U);

    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }

    oReturnCheck = Fapi_setActiveFlashBank(Fapi_FlashBank0);

    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }

    return 0;
}

/**
 * @brief Clear FSM status
 */
#pragma CODE_SECTION(ClearFSMStatus, ".TI.ramfunc");
void ClearFSMStatus(void)
{
    Fapi_FlashStatusType oFlashStatus;
    Fapi_StatusType oReturnCheck;

    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady)
    {
    }

    oFlashStatus = Fapi_getFsmStatus();
    if (oFlashStatus != 0)
    {
        oReturnCheck = Fapi_issueAsyncCommand(Fapi_ClearStatus);

        while (Fapi_getFsmStatus() != 0)
        {
        }

        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
        }
    }
}

/**
 * @brief Program flash with Auto ECC
 */
#pragma CODE_SECTION(Example_ProgramUsingAutoECC, ".TI.ramfunc");

void Example_ProgramUsingAutoECC(void)
{
    uint32 u32Index = 0;
    uint16 i = 0;
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;

    for (i = 0, u32Index = addressToWrite;
         (u32Index < (addressToWrite + datasize)); i += 8, u32Index += 8)
    {
        ClearFSMStatus();

        Fapi_setupBankSectorEnable(
                FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, SEC0TO31);
        Fapi_setupBankSectorEnable(
                FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, SEC32To127);

        oReturnCheck = Fapi_issueProgrammingCommand((uint32*) u32Index,
                                                    Buffer + i, 8, 0, 0,
                                                    Fapi_AutoEccGeneration);

        while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy)
            ;

        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
        }

        oFlashStatus = Fapi_getFsmStatus();
        if (oFlashStatus != 3)
        {
            FMSTAT_Fail();
        }

        oReturnCheck = Fapi_doVerify((uint32*) u32Index, 4,
                                     (uint32*) (uint32) (Buffer + i),
                                     &oFlashStatusWord);

        Fapi_setupBankSectorEnable(
                FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0xFFFFFFFF);

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

/**
 * @brief Erase a single sector
 */
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
                                                     (uint32*) addressToWrite);

    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady)
    {
    }

    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }

    oFlashStatus = Fapi_getFsmStatus();
    if (oFlashStatus != 3)
    {
        FMSTAT_Fail();
    }

    oReturnCheck = Fapi_doBlankCheck((uint32*) addressToWrite,
                                     Sector2KB_u32length,
                                     &oFlashStatusWord);
    if (oReturnCheck != Fapi_Status_Success)
    {
        Example_Error(oReturnCheck);
    }
}

/**
 * @brief Erase entire Bank 4
 */
void eraseBank4(void)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    Fapi_FlashStatusWordType oFlashStatusWord;
    uint32 u32CurrentAddress = 0;

    ClearFSMStatus();

    // Enable program/erase protection for select sectors
    // CMDWEPROTA is for sectors 0-31
    // Bits 0-11 of CMDWEPROTB is for sectors 32-127, each bit represents
    // a group of 8 sectors
    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA,
                               0x00000000);
    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB,
                               0xFFFFFFFF);

    u32CurrentAddress = FlashBank4StartAddress;

    // Issue bank erase command
    oReturnCheck = Fapi_issueBankEraseCommand((uint32*) u32CurrentAddress);

    // Wait until FSM is done with erase sector operation
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady)
    {
    }

    if (oReturnCheck != Fapi_Status_Success)
    {
        // Check Flash API documentation for possible errors
        Example_Error(oReturnCheck);
    }

    // Read FMSTAT register contents to know the status of FSM after
    // erase command to see if there are any erase operation related errors
    oFlashStatus = Fapi_getFsmStatus();
    if (oFlashStatus != 3)
    {
        // Check Flash API documentation for FMSTAT and debug accordingly
        // Fapi_getFsmStatus() function gives the FMSTAT register contents.
        // Check to see if any of the EV bit, ESUSP bit, CSTAT bit or
        // VOLTSTAT bit is set (Refer to API documentation for more details).
        FMSTAT_Fail();
    }

    // Verify that Bank 4 is erased.
    // First check first 8 sectors
    u32CurrentAddress = FlashBank4StartAddress;
    oReturnCheck = Fapi_doBlankCheck((uint32*) u32CurrentAddress,
                                     (8 * Sector2KB_u32length),
                                     &oFlashStatusWord);

    if (oReturnCheck != Fapi_Status_Success)
    {
        // Check Flash API documentation for error info
        Example_Error(oReturnCheck);
    }

    // Then check remaining sectors
    u32CurrentAddress = FlashBank4StartAddress + 0xC000;
    oReturnCheck = Fapi_doBlankCheck((uint32*) u32CurrentAddress,
                                     (80 * Sector2KB_u32length),
                                     &oFlashStatusWord);

    if (oReturnCheck != Fapi_Status_Success)
    {
        // Check Flash API documentation for error info
        Example_Error(oReturnCheck);
    }
}

/**
 * @brief Handle flash programming error
 * @param status Error status code
 */
void Example_Error(Fapi_StatusType status)
{
    updateFailed = true;
}

/**
 * @brief Handle successful flash operation
 */
void Example_Done(void)
{
    updateFailed = false;
}

/**
 * @brief Handle FMSTAT failure
 */
void FMSTAT_Fail(void)
{
    updateFailed = true;
}

/**
 * @brief Handle ECC failure
 */
void ECC_Fail(void)
{
    // This is typically a serious error in flash programming
    __asm("    ESTOP0");
}

/**
 * @brief Find read and write addresses in flash
 */
void findReadAndWriteAddress(void)
{
    // Local Variables
    uint32_t addr;
    uint16_t val;

    newestData = 0;
    oldestData = 0xFFFF;
    writeAddress = 0xFFFFFFFF;
    readAddress = BANK4_START;
    emptySectorFound = false;

    for (addr = BANK4_START; addr < BANK4_END; addr += SECTOR_SIZE)
    {
        val = *(volatile uint16_t*) addr;

        if (val == 0xFFFF)
        {
            emptySectorFound = true;
            continue; // Empty sector
        }

        // Track newest
        if (val <= WRAP_LIMIT && val > newestData)
        {
            newestData = val;
            readAddress = addr;
        }

        // Track oldest
        if (val <= WRAP_LIMIT && val < oldestData && val > 0)
        {
            oldestData = val;
            writeAddress = addr;
        }
    }
}

/**
 * @brief Find an empty sector or erase oldest sector
 */
void eraseSectorOrFindEmptySector(void)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusWordType oFlashStatusWord;
    uint32_t i;

    // To find an empty sector
    if (emptySectorFound)
    {
        for (i = BANK4_START; i < BANK4_END; i += SECTOR_SIZE)
        {
            oReturnCheck = Fapi_doBlankCheck((uint32*) i,
                                             Sector2KB_u32length,
                                             &oFlashStatusWord);
            if (oReturnCheck == Fapi_Status_Success)
            {
                // Found an empty sector
                addressToWrite = i;
                return;
            }
        }
    }

    if ((oldestData < WRAP_LIMIT) && (newestData != WRAP_LIMIT))
    {
        addressToWrite = writeAddress;
        Example_EraseSector();  // Erase the oldest sector
        return;
    }

    // If all else fails (we're at the wrap limit), erase bank 4
    /*
    if (newestData == WRAP_LIMIT || oldestData == WRAP_LIMIT)
    {
        eraseBank4();
        newestData = 0;
        oldestData = 0xFFFF;
        writeAddress = 0xFFFFFFFF;
        addressToWrite = BANK4_START;
        return;
    }
    */
}

/**
 * @brief Enable flash write capability
 * @return int Status code
 */
int enableFlashWrite(void)
{
    int i;
    uint16_t expectedData[64] = {
        2, 0x44, 0x65, 0x6c, 0x74, 0x61, 0x40, 0x50,
        0x44, 0x55, 0x47, 0x45, 0x4e, 0x33, 0x2e,
        0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    for (i = 0; i < 64; i++)
    {
        if (rxMsg[0].data[i] == expectedData[i])
        {
            flashWriteEnabled = true;
        }
    }

    if (flashWriteEnabled == true)
    {
        flashWriteEnabledAck();
    }

    return 0;
}

/**
 * @brief Send success acknowledgment for calibration data update
 */
void calibDataUpdateSuccessAck(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
    TxMsg.id = ((uint32_t) (0x4)) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    uint8_t datare[] = { 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00 };

    int i;
    for (i = 0; i < 64; i++)
        {
            TxMsg.data[i] = datare[i];
        }

        MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

        // Add transmission request for Tx buffer 0
        MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

        // Wait for transmission (with timeout)
        canTxWaitComplete(CAN_MBOARD_BASE);
    }

    /**
     * @brief Send failure acknowledgment for calibration data update
     */
    void calibDataUpdateFailAck(void)
    {
        MCAN_TxBufElement TxMsg;
        memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
        TxMsg.id = ((uint32_t) (0x4)) << 18U;
        TxMsg.brs = 0x1;
        TxMsg.dlc = 15;
        TxMsg.rtr = 0;
        TxMsg.xtd = 0;
        TxMsg.fdf = 0x1;
        TxMsg.esi = 0U;
        TxMsg.efc = 1U;
        TxMsg.mm = 0xAAU;

        uint8_t datare[64] = { 0x04, 0x00 };

        int i;
        for (i = 0; i < 64; i++)
        {
            TxMsg.data[i] = datare[i];
        }

        MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

        // Add transmission request for Tx buffer 0
        MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

        // Wait for transmission (with timeout)
        canTxWaitComplete(CAN_MBOARD_BASE);
    }

    /**
     * @brief Send acknowledgment for flash write enable
     */
    void flashWriteEnabledAck(void)
    {
        MCAN_TxBufElement TxMsg;
        memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
        TxMsg.id = ((uint32_t) (0x4)) << 18U;
        TxMsg.brs = 0x1;
        TxMsg.dlc = 15;
        TxMsg.rtr = 0;
        TxMsg.xtd = 0;
        TxMsg.fdf = 0x1;
        TxMsg.esi = 0U;
        TxMsg.efc = 1U;
        TxMsg.mm = 0xAAU;

        uint8_t datare[64] = { 0x02, 0x01 };
        int i;
        for (i = 0; i < 64; i++)
        {
            TxMsg.data[i] = datare[i];
        }

        MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

        // Add transmission request for Tx buffer 0
        MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

        // Wait for transmission (with timeout)
        canTxWaitComplete(CAN_MBOARD_BASE);
    }

    /**
     * @brief Send acknowledgment for flash write disable
     */
    void flashWriteDisabledAck(void)
    {
        MCAN_TxBufElement TxMsg;
        memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
        TxMsg.id = ((uint32_t) (0x4)) << 18U;
        TxMsg.brs = 0x1;
        TxMsg.dlc = 15;
        TxMsg.rtr = 0;
        TxMsg.xtd = 0;
        TxMsg.fdf = 0x1;
        TxMsg.esi = 0U;
        TxMsg.efc = 1U;
        TxMsg.mm = 0xAAU;

        // 00 - Disabled
        uint8_t datare[64] = { 0x02, 0x00 };
        int i;
        for (i = 0; i < 64; i++)
        {
            TxMsg.data[i] = datare[i];
        }

        MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

        // Add transmission request for Tx buffer 0
        MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

        // Wait for transmission (with timeout)
        canTxWaitComplete(CAN_MBOARD_BASE);
    }

    /**
     * @brief Send acknowledgment for Bank 4 erase
     */
    void erasedBank4Ack(void)
    {
        MCAN_TxBufElement TxMsg;
        memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
        TxMsg.id = ((uint32_t) (0x4)) << 18U;
        TxMsg.brs = 0x1;
        TxMsg.dlc = 15;
        TxMsg.rtr = 0;
        TxMsg.xtd = 0;
        TxMsg.fdf = 0x1;
        TxMsg.esi = 0U;
        TxMsg.efc = 1U;
        TxMsg.mm = 0xAAU;

        uint8_t datare[64] = { 0x09, 0x01 };
        int i;
        for (i = 0; i < 64; i++)
        {
            TxMsg.data[i] = datare[i];
        }

        MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

        // Add transmission request for Tx buffer 0
        MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

        // Wait for transmission (with timeout)
        canTxWaitComplete(CAN_MBOARD_BASE);
    }

    /**
     * @brief Send acknowledgment for default calibration values written
     */
    void writtenDefaultCalibrationValuesAck(void)
    {
        MCAN_TxBufElement TxMsg;
        memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
        TxMsg.id = ((uint32_t) (0x4)) << 18U;
        TxMsg.brs = 0x1;
        TxMsg.dlc = 15;
        TxMsg.rtr = 0;
        TxMsg.xtd = 0;
        TxMsg.fdf = 0x1;
        TxMsg.esi = 0U;
        TxMsg.efc = 1U;
        TxMsg.mm = 0xAAU;

        uint8_t datare[64] = { 0xA, 0x1 };
        int i;
        for (i = 0; i < 64; i++)
        {
            TxMsg.data[i] = datare[i];
        }

        MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

        // Add transmission request for Tx buffer 0
        MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

        // Wait for transmission (with timeout)
        canTxWaitComplete(CAN_MBOARD_BASE);
    }
