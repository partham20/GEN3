/**
 * @file pdu_manager.c
 * @brief PDU data structures and management functions implementation
 *
 * Wire format (S-Board <-> M-Board): PDUData (451 words)
 * Flash format: sectorIndex(1) + PDUData(451) = 452 words
 */

#include "pdu_manager.h"
#include "flash_driver.h"
#include "bu_board.h"
#include "can_driver.h"
#include "timer_driver.h"

// Global variables
PDUDataManager pduManager;
CANFrame storedFrames[NUM_FRAMES];
uint16_t receivedBuffer[TOTAL_BUFFER_SIZE];

void initializePDUManager(void)
{
    memset(&pduManager.readWriteData, 0, sizeof(CalibrationData));
    memset(&pduManager.workingData, 0, sizeof(CalibrationData));
    pduManager.dataSync = true;
}

void syncWorkingToReadWrite(void)
{
    memcpy(&pduManager.readWriteData, &pduManager.workingData,
           sizeof(CalibrationData));
    pduManager.dataSync = true;
}

void syncReadWriteToWorking(void)
{
    memcpy(&pduManager.workingData, &pduManager.readWriteData,
           sizeof(CalibrationData));
    pduManager.dataSync = true;
}

bool updateWorkingData(const CalibrationData *newData)
{
    if (newData == NULL)
    {
        return false;
    }
    memcpy(&pduManager.workingData, newData, sizeof(CalibrationData));
    pduManager.dataSync = false;
    return true;
}

/**
 * @brief Write default calibration values to flash.
 * All gains default to 10000 (= 1.0x multiplier).
 */
void writeDefaultCalibrationValues(void)
{
    if (newestData >= WRAP_LIMIT)
    {
        eraseBank4();
        newestData = 0;
        oldestData = 0xFFFF;
        writeAddress = 0xFFFFFFFF;
        addressToWrite = BANK4_START;
    }

    PDUData defaultPduData = {
        .primaryVoltage     = { .R = 10000, .Y = 10000, .B = 10000 },
        .secondaryVoltage   = { .R = 10000, .Y = 10000, .B = 10000 },
        .primaryCurrent     = { .R = 10000, .Y = 10000, .B = 10000 },
        .secondaryCurrent   = { .R = 10000, .Y = 10000, .B = 10000 },
        .primaryKW          = { .R = 10000, .Y = 10000, .B = 10000 },
        .secondaryKW        = { .R = 10000, .Y = 10000, .B = 10000 },
        .groundCurrent      = 10000
    };

    int i;
    for (i = 0; i < numberOfCTs; i++)
    {
        defaultPduData.gains.ctGain[i] = 10000;
        defaultPduData.gains.kwGain[i] = 10000;
    }

    pduManager.readWriteData.sectorIndex = ++newestData;
    pduManager.readWriteData.pduData = defaultPduData;

    memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibrationData));
    datasize = sizeof(CalibrationData) / sizeof(uint16_t);

    pduManager.dataSync = false;
}

/**
 * @brief Validate and sanitize calibration gains loaded from flash.
 * Valid gains: 1-50000 (0.0001x to 5.0x). Out-of-range values reset to 10000 (1.0x).
 */
void validateCalibrationGains(void)
{
    PDUData *pd = &pduManager.readWriteData.pduData;
    uint16_t *fields[] = {
        &pd->primaryVoltage.R, &pd->primaryVoltage.Y, &pd->primaryVoltage.B,
        &pd->secondaryVoltage.R, &pd->secondaryVoltage.Y, &pd->secondaryVoltage.B,
        &pd->primaryCurrent.R, &pd->primaryCurrent.Y, &pd->primaryCurrent.B,
        &pd->secondaryCurrent.R, &pd->secondaryCurrent.Y, &pd->secondaryCurrent.B,
        &pd->primaryKW.R, &pd->primaryKW.Y, &pd->primaryKW.B,
        &pd->secondaryKW.R, &pd->secondaryKW.Y, &pd->secondaryKW.B,
        &pd->groundCurrent
    };

    int i;
    for (i = 0; i < 19; i++)
    {
        if (*fields[i] == 0 || *fields[i] > 50000)
        {
            *fields[i] = 10000;
        }
    }
}

/**
 * @brief Serialize PDUData into CAN frames (storedFrames[]).
 * Sends PDUData (451 words) as the wire payload.
 */
void prepareCalibrationFramesForTx(const PDUData *pduData)
{
    prepareFramesForTransmission((uint16_t *)pduData, calibrationdDataSize);
}

/**
 * @brief Pack a 16-bit word buffer into CAN-FD frames.
 * Each frame: 2-byte order header + up to 62 data bytes (31 words).
 * Words are stored big-endian (MSB first) in the frame data bytes.
 */
void prepareFramesForTransmission(uint16_t *buffer, uint16_t bufferSizeWords)
{
    uint32_t remainingWords = bufferSizeWords;
    uint32_t bufferOffset = 0;
    uint32_t j;
    uint16_t i, value;

    for (i = 0; i < NUM_FRAMES && remainingWords > 0; i++)
    {
        CANFrame *frame = &storedFrames[i];
        frame->order = i + 1;

        // 2-byte order header
        frame->data[0] = (frame->order >> 8) & 0xFF;
        frame->data[1] = frame->order & 0xFF;

        // Pack up to 31 words (62 bytes) into frame
        uint32_t wordsThisFrame = (remainingWords < 31) ? remainingWords : 31;
        for (j = 0; j < wordsThisFrame; j++)
        {
            value = buffer[bufferOffset + j];
            frame->data[2 + j * 2] = (value >> 8) & 0xFF;
            frame->data[2 + j * 2 + 1] = value & 0xFF;
        }

        // Zero-fill remaining bytes in last frame
        for (j = wordsThisFrame; j < 31; j++)
        {
            frame->data[2 + j * 2] = 0;
            frame->data[2 + j * 2 + 1] = 0;
        }

        bufferOffset += wordsThisFrame;
        remainingWords -= wordsThisFrame;
    }
}

/**
 * @brief Process received calibration data from M-Board.
 *
 * receivedBuffer[] must already contain the reassembled wire data (451 words)
 * from copyRxFIFO(). This function:
 *   1. Copies PDUData into readWriteData using memcpy (no manual index mapping)
 *   2. Finds flash sector, sets sectorIndex
 *   3. Writes CalibrationData (452 words) to flash
 *   4. On success: syncs to working, relays to BU boards, sends ACK
 *   5. On failure: sends fail ACK
 */
void processReceivedCalibrationData(void)
{
    // 1. Copy PDUData from receivedBuffer into readWriteData
    //    receivedBuffer layout matches PDUData struct memory layout exactly:
    //      words 0..215   = ctGain[216]
    //      words 216..431 = kwGain[216]
    //      words 432..434 = primaryVoltage (R,Y,B)
    //      words 435..437 = secondaryVoltage (R,Y,B)
    //      words 438..440 = primaryCurrent (R,Y,B)
    //      words 441..443 = secondaryCurrent (R,Y,B)
    //      words 444..446 = primaryKW (R,Y,B)
    //      words 447..449 = secondaryKW (R,Y,B)
    //      word  450      = groundCurrent
    memcpy(&pduManager.readWriteData.pduData, receivedBuffer, sizeof(PDUData));

    // 2. Find flash address and handle wrap limit
    findReadAndWriteAddress();

    if (newestData >= WRAP_LIMIT)
    {
        eraseBank4();
        newestData = 0;
        oldestData = 0xFFFF;
        writeAddress = 0xFFFFFFFF;
        addressToWrite = BANK4_START;
    }

    eraseSectorOrFindEmptySector();

    // 3. Set sectorIndex, copy to flash buffer
    pduManager.readWriteData.sectorIndex = ++newestData;

    memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibrationData));
    datasize = sizeof(CalibrationData) / sizeof(uint16_t);

    // 4. Write to flash
    Example_ProgramUsingAutoECC();

    // 5. Handle result
    if (!updateFailed)
    {
        syncReadWriteToWorking();
        copyFlashToBUBoards();

        bool allBUAcked = sendAllBoardsCalibrationDataWithRetry();
        if (allBUAcked)
        {
            calibDataUpdateSuccessAck();
        }
        else
        {
            calibDataUpdateFailAck();
        }
    }
    else
    {
        calibDataUpdateFailAck();
    }

    resetFrameTracking();
    retryTimerExpired = false;
}
