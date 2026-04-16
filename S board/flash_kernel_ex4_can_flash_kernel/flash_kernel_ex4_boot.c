//###########################################################################
//
// FILE:    flash_kernel_ex4_boot.c
//
// TITLE:   Boot loader shared functions
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
#include "ex_4_cpu1brom_utils.h"

#include "flash_programming_f28p55x.h"
#include "FlashTech_F28P55x_C28x.h"
#include "flash_kernel_ex3_commands.h"

#include "driverlib.h"

typedef struct
{
    uint16_t status;
    uint32_t address;
    uint32_t data;
    uint16_t flashAPIError;
    uint32_t flashAPIFsmStatus;
} StatusCode;
extern StatusCode statusCode;

#define BUFFER_SIZE               0x20

#define LOWER_FIRST_BLOCK_SIZE      22U
#define UPPER_FIRST_BLOCK_SIZE      23U

#define DCSM_OTP_Z1_LNKPTR_START 0x78000
#define DCSM_OTP_Z1_LNKPTR_END   0x78020

#define DCSM_OTP_Z2_LNKPTR_START 0x78200
#define DCSM_OTP_Z2_LNKPTR_END   0x78220

//
// GetWordData is a pointer to the function that interfaces to the peripheral.
// Each loader assigns this pointer to it's particular GetWordData function.
//
uint16fptr GetWordData;

//
// miniBuffer: Useful for 4-word access to flash
//
uint16_t miniBuffer[4];

//
// Function prototypes
//
uint32_t GetLongData();
void CopyData(void);
void ReadReservedFn(void);
extern void sharedErase(uint32_t sectors);
void DCSM_OTP_Write(uint32_t destAddr, uint16_t* miniBuffer, uint32_t WE_Protection_Mask_UO);
extern uint32_t MCAN_getDataFromBuffer(MCAN_dataTypeSize dataTypeSize);
extern uint16_t msgBufferIndex;
void ClearFSMStatus();
void SetFlashAPIError(Fapi_StatusType status);
void Example_Error();

//
// CopyApplication - This routine copies multiple blocks of data from the host
//                   to the specified Flash locations. It is assumed that the
//                   application is linked to Flash correctly and that the image is
//                   128 bit aligned. Errors from the Flash API are not currently
//                   being relayed to the host.
//
//                   Multiple blocks of data are copied until a block
//                   size of 00 00 is encountered.
//

void CopyApplication(MCAN_RxBufElement rxMsg, uint32_t* CMD_WE_Protection_A_Masks,
                     uint32_t* CMD_WE_Protection_B_Masks, uint32_t* CMD_WE_Protection_UO_Masks)
{
    struct HEADER {
      uint16_t BlockSize;
      uint32_t DestAddr;
    } BlockHeader;

    uint16_t i = 0;
    uint16_t j = 0;
    uint16_t k = 0;
    uint16_t fail = 0;
    uint16_t wordsWritten = 0;

    //
    // wordData: Stores a word of data
    //
    uint16_t wordData;

    //
    // Buffer: Used to program data to flash
    //
    uint16_t Buffer[BUFFER_SIZE];

    //
    // Get the size in words of the first block
    //
    BlockHeader.BlockSize = BUILD_WORD(rxMsg.data[LOWER_FIRST_BLOCK_SIZE],
                                       rxMsg.data[UPPER_FIRST_BLOCK_SIZE]);

    //
    // Set the message buffer index for reading next stream data
    //
    msgBufferIndex = UPPER_FIRST_BLOCK_SIZE + 1U;

    //
    // While the block size is > 0 flash the data
    // to the DestAddr.  There is no error checking
    // as it is assumed the DestAddr is a valid
    // memory location
    //
    while(BlockHeader.BlockSize != (uint16_t)0x0000)
    {
        Fapi_StatusType oReturnCheck;
        Fapi_FlashStatusWordType oFlashStatusWord;
        Fapi_FlashStatusType oFlashStatus;

        BlockHeader.DestAddr = MCAN_getDataFromBuffer(MCAN_DATA_SIZE_32BITS);

        // Find current bank to use appropriate protection masks
        uint16_t currentBank = 0;
        uint16_t otpFlag = 0;

        if (BlockHeader.DestAddr >= FlashBank0StartAddress && BlockHeader.DestAddr < FlashBank1StartAddress) 
        {
            currentBank = 0;
        } else if (BlockHeader.DestAddr >= FlashBank1StartAddress && BlockHeader.DestAddr < FlashBank2StartAddress)
        {
            currentBank = 1;
        } else if (BlockHeader.DestAddr >= FlashBank2StartAddress && BlockHeader.DestAddr < FlashBank3StartAddress)
        {
            currentBank = 2;
        } else if (BlockHeader.DestAddr >= FlashBank3StartAddress && BlockHeader.DestAddr < FlashBank4StartAddress)
        {
            currentBank = 3;
        } else if (BlockHeader.DestAddr >= FlashBank4StartAddress && BlockHeader.DestAddr <= FlashBank4EndAddress)
        {
            currentBank = 4;
        }
        // Bank 0 OTP
        else if ((BlockHeader.DestAddr >= Bank0OTP_MAP_BEGIN) && (BlockHeader.DestAddr <= Bank0OTP_MAP_END))
        {
            otpFlag = 1;
            currentBank = 0;
        }
        // Bank 1 OTP
        else if ((BlockHeader.DestAddr >= Bank1OTP_MAP_BEGIN) && (BlockHeader.DestAddr <= Bank1OTP_MAP_END))
        {
            otpFlag = 1;
            currentBank = 1;
        } 
        // Bank 2 OTP
        else if ((BlockHeader.DestAddr >= Bank2OTP_MAP_BEGIN) && (BlockHeader.DestAddr <= Bank2OTP_MAP_END))
        {
            otpFlag = 1;
            currentBank = 2;
        }
        // Bank 3 OTP
        else if ((BlockHeader.DestAddr >= Bank3OTP_MAP_BEGIN) && (BlockHeader.DestAddr <= Bank3OTP_MAP_END))
        {
            otpFlag = 1;
            currentBank = 3;
        }
        // Bank 4 OTP
        else if ((BlockHeader.DestAddr >= Bank4OTP_MAP_BEGIN) && (BlockHeader.DestAddr <= Bank4OTP_MAP_END))
        {
            otpFlag = 1;
            currentBank = 4;
        } 

        //
        // Iterate through the block of data in order to program the data
        // in flash
        //
        for (i = 0; i < BlockHeader.BlockSize; i += 0)
        {
            //
            // If the size of the block of data is less than the size of the buffer,
            // then fill the buffer with the block of data and pad the remaining
            // elements
            //
            if (BlockHeader.BlockSize < BUFFER_SIZE)
            {
                //
                // Receive the block of data one word at a time and place it in
                // the buffer
                //
                for (j = 0; j < BlockHeader.BlockSize; j++)
                {
                    //
                    // Receive one word of data
                    //
                    wordData = (uint16_t)(MCAN_getDataFromBuffer(MCAN_DATA_SIZE_16BITS));

                    //
                    // Put the word of data in the buffer
                    //
                    Buffer[j] = wordData;

                    //
                    // Increment i to keep track of how many words have been received
                    //
                    i++;
                }

                //
                // Pad the remaining elements of the buffer
                //
                for (j = BlockHeader.BlockSize; j < BUFFER_SIZE; j++)
                {
                    //
                    // Put 0xFFFF into the current element of the buffer. OxFFFF is equal to erased
                    // data and has no effect
                    //
                    Buffer[j] = 0xFFFF;
                }
            }

            //
            // Block is to big to fit into our buffer so we must program it in
            // chunks
            //
            else
            {
                //
                // Less than one BUFFER_SIZE left
                //
                if ((BlockHeader.BlockSize - i) < BUFFER_SIZE)
                {
                    //
                    // Fill Buffer with rest of data
                    //
                    for (j = 0; j < BlockHeader.BlockSize - i; j++)
                    {
                        //
                        // Receive one word of data
                        //
                        wordData = (uint16_t)(MCAN_getDataFromBuffer(MCAN_DATA_SIZE_16BITS));

                        //
                        // Put the word of data into the current element of Buffer
                        //
                        Buffer[j] = wordData;
                    }

                    //
                    // Increment i outside here so it doesn't affect loop above
                    //
                    i += j;

                    //
                    // Fill the rest with 0xFFFF
                    //
                    for (; j < BUFFER_SIZE; j++)
                    {
                        Buffer[j] = 0xFFFF;
                    }
                }
                else
                {
                    //
                    // Fill up like normal, up to BUFFER_SIZE
                    //
                    for (j = 0; j < BUFFER_SIZE; j++)
                    {
                        wordData = (uint16_t)(MCAN_getDataFromBuffer(MCAN_DATA_SIZE_16BITS));
                        Buffer[j] = wordData;
                        i++;
                    }
                }
            }

            // If writing to 512 bits of DCSM OTP containing link pointers, write 64-bits at a time
            if ((BlockHeader.DestAddr >= DCSM_OTP_Z1_LNKPTR_START) && (BlockHeader.DestAddr < DCSM_OTP_Z1_LNKPTR_END) || 
            (BlockHeader.DestAddr >= DCSM_OTP_Z2_LNKPTR_START) && (BlockHeader.DestAddr < DCSM_OTP_Z2_LNKPTR_END))
            {

                //
                // Fill miniBuffer with the data in Buffer in order to program the data
                // to flash; miniBuffer takes data from Buffer, s4 words at a time
                //
                for (k = 0; k < (BUFFER_SIZE / 4); k++)
                {
                    if (fail == 0)
                    {
                        uint16_t bufferOffset = k * 4;

                        miniBuffer[0] = Buffer[bufferOffset + 0];
                        miniBuffer[1] = Buffer[bufferOffset + 1];
                        miniBuffer[2] = Buffer[bufferOffset + 2];
                        miniBuffer[3] = Buffer[bufferOffset + 3];

                        //
                        // check that all the words have not already been written
                        //
                        if (wordsWritten < BlockHeader.BlockSize)
                        {
                            ClearFSMStatus();

                            uint16_t DCSM_OTP_Mask = CMD_WE_Protection_UO_Masks[0];
                            DCSM_OTP_Write(BlockHeader.DestAddr, miniBuffer, DCSM_OTP_Mask);

                        } //check if all the words are not already written
                    }

                    BlockHeader.DestAddr += 0x4;
                    wordsWritten += 0x4;
                } //for(int k); loads miniBuffer with Buffer elements
            } 
            else
            {
                //
                // check that all the words have not already been written
                //
                if (wordsWritten < BlockHeader.BlockSize)
                {
                    if(fail == 0)
                    {
                        // Clear Fsm Status before writing
                        ClearFSMStatus();

                        //
                        //program 32 words at once, 512-bits
                        //
                        //

                        // Set appropriate protection mask based on where we are writing
                        if (otpFlag)
                        {
                            Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE+FLASH_O_CMDWEPROT_UO, CMD_WE_Protection_UO_Masks[currentBank]);
                        } else 
                        {
                            //
                            // Bits 0-11 of CMDWEPROTB is applicable for sectors 32-127, each bit represents
                            // a group of 8 sectors, e.g bit 0 represents sectors 32-39, bit 1 represents
                            // sectors 40-47, etc
                            //
                            Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE+FLASH_O_CMDWEPROTA, CMD_WE_Protection_A_Masks[currentBank]);
                            Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE+FLASH_O_CMDWEPROTB, CMD_WE_Protection_B_Masks[currentBank]);

                        }

                        oReturnCheck = Fapi_issueAutoEcc512ProgrammingCommand((uint32_t *) BlockHeader.DestAddr,
                                                                                Buffer, sizeof(Buffer));

                        while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy);

                        oFlashStatus = Fapi_getFsmStatus();
                        if (oReturnCheck != Fapi_Status_Success || oFlashStatus != 3)
                        {
                            statusCode.status = PROGRAM_ERROR;
                            statusCode.address = BlockHeader.DestAddr;
                            SetFlashAPIError(oReturnCheck);
                            statusCode.flashAPIFsmStatus = oFlashStatus;

                            fail++;
                        }
                    }

                    // Only verify if we are not writing to DCSM
                    if (!((BlockHeader.DestAddr >= (Bank0OTP_MAP_BEGIN + 0x20)) && (BlockHeader.DestAddr <= Bank0OTP_MAP_END))) 
                    {
                        
                        oReturnCheck = Fapi_doVerify((uint32_t *) (BlockHeader.DestAddr), 16, (uint32_t*) Buffer, &oFlashStatusWord);
                        if (oReturnCheck != Fapi_Status_Success) 
                        {
                            statusCode.status = VERIFY_ERROR;
                            statusCode.address = oFlashStatusWord.au32StatusWord[0];
                            SetFlashAPIError(oReturnCheck);
                            //
                            // FMSTAT not checked for Verify
                            //
                            statusCode.flashAPIFsmStatus = 0;
                            fail++;
                        }

                    }

                } //check if all the words are not already written
                BlockHeader.DestAddr += 0x20;
                wordsWritten += 0x20;

            }
        }

        // Check if failure was encountered earlier
        if (fail != 0)
        {
            Example_Error();
        }


        //
        // Get the size of the next block
        //
        BlockHeader.BlockSize = (uint16_t)(MCAN_getDataFromBuffer(MCAN_DATA_SIZE_16BITS));
        wordsWritten = 0;
    }
    return;
}

// DCSM_OTP_Write is used to write 64-bits to DCSM OTP 
// This is needed because we cannot write 128 or 512 bits to the link pointers
void DCSM_OTP_Write(uint32_t destAddr, uint16_t miniBuffer[], uint32_t WE_Protection_Mask_UO)
{
    Fapi_FlashStatusType oFlashStatus;

    //
    // Error return variable
    //
    Fapi_StatusType oReturnCheck;

    //
    //program 4 words at once, 64-bits
    //
    //

    //
    // Disable erase/program protection
    Fapi_setupBankSectorEnable(FLASH_WRAPPER_PROGRAM_BASE+FLASH_O_CMDWEPROT_UO, WE_Protection_Mask_UO);

    oReturnCheck = Fapi_issueProgrammingCommand((uint32_t *) destAddr, miniBuffer,
                                                4, 0, 0, Fapi_AutoEccGeneration);

    while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy);

    oFlashStatus = Fapi_getFsmStatus();

    if (oReturnCheck != Fapi_Status_Success || oFlashStatus != 3)
    {
        Example_Error();
    }
   
}

//
// getLongData -    Fetches a 32-bit value from the peripheral
//                  input stream.
//

uint32_t GetLongData(void)
{
    uint32_t longData;

    //
    // Fetch the upper 1/2 of the 32-bit value
    //
    longData = ( (uint32_t)(*GetWordData)() << 16);

    //
    // Fetch the lower 1/2 of the 32-bit value
    //
    longData |= (uint32_t)(*GetWordData)();

    return longData;
}

//
// Read_ReservedFn -    Reads 8 reserved words in the header.
//                      None of these reserved words are used by the
//                      this boot loader at this time, they may be used in
//                      future devices for enhancments.  Loaders that use
//                      these words use their own read function.
//

void ReadReservedFn(void)
{
    uint16_t i;

    //
    // Read and discard the 8 reserved words.
    //
    for(i = 1; i <= 8; i++)
    {
       GetWordData();
    }
    return;
}

void ClearFSMStatus()
{

    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;

    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady){}
    oFlashStatus = Fapi_getFsmStatus();
    if(oFlashStatus != 0)
    {

        /* Clear the Status register */
        oReturnCheck = Fapi_issueAsyncCommand(Fapi_ClearStatus);
        //
        // Wait until status is cleared
        //
        while (Fapi_getFsmStatus() != 0) {}

        if(oReturnCheck != Fapi_Status_Success)
        {
            statusCode.status = CLEAR_STATUS_ERROR;
            statusCode.address = 0;
            SetFlashAPIError(oReturnCheck);
            statusCode.flashAPIFsmStatus = 0; // not used here
            Example_Error();
        }
    }
}


//
// void setFlashAPIError(Fapi_StatusType status) - sets Flash API error from
//                                                 operation into struct to
//                                                 display on console
//
void SetFlashAPIError(Fapi_StatusType status)
{
    if(status == Fapi_Error_AsyncIncorrectDataBufferLength)
    {
        statusCode.flashAPIError = INCORRECT_DATA_BUFFER_LENGTH;
    }
    else if(status == Fapi_Error_AsyncIncorrectEccBufferLength)
    {
        statusCode.flashAPIError = INCORRECT_ECC_BUFFER_LENGTH;
    }
    else if(status == Fapi_Error_AsyncDataEccBufferLengthMismatch)
    {
        statusCode.flashAPIError = DATA_ECC_BUFFER_LENGTH_MISMATCH;
    }
    else if(status == Fapi_Error_FlashRegsNotWritable)
    {
        statusCode.flashAPIError = FLASH_REGS_NOT_WRITABLE;
    }
    else if(status == Fapi_Error_FeatureNotAvailable)
    {
        statusCode.flashAPIError = FEATURE_NOT_AVAILABLE;
    }
    else if(status == Fapi_Error_InvalidAddress)
    {
        statusCode.flashAPIError = INVALID_ADDRESS;
    }
    else if(status == Fapi_Error_Fail)
    {
        statusCode.flashAPIError = FAILURE;
    }
    else
    {
        statusCode.status = NOT_RECOGNIZED;
    }
}

void Example_Error()
{
    asm(" ESTOP0");
}

// End of file
