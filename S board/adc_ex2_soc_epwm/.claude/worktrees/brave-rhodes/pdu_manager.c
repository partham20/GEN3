/**
 * @file pdu_manager.c
 * @brief PDU data structures, calibration management, and flash persistence
 *
 * Handles the full lifecycle of calibration data:
 *   1. Receive from M-Board (wire format) --> validate CRC --> parse into struct
 *   2. Compute flash CRC --> write struct to flash Bank 4
 *   3. Read from flash --> validate CRC --> load into working struct
 *   4. Pack struct into CAN frames for M-Board readback
 *
 * Wire format vs struct layout:
 *   The M-Board sends 19 S-board parameters in interleaved order
 *   (pri volt, sec volt, pri curr, sec curr, ...).  The PDUData struct
 *   groups them (all primary, then all secondary).  RxBufferToStruct()
 *   and structToWireBuffer() handle the mapping explicitly.
 */

#include "pdu_manager.h"
#include "flash_driver.h"

/* ============================================================
 * Global Variables
 * ============================================================ */

PDUDataManager pduManager;
CANFrame storedFrames[NUM_FRAMES];
uint16_t receivedBuffer[TOTAL_BUFFER_SIZE];

/* ============================================================
 * Initialization
 * ============================================================ */

/**
 * @brief Zero-initialize both PDU manager copies.
 * Call this once at startup before loading from flash.
 */
void initializePDUManager(void)
{
    memset(&pduManager.readWriteData, 0, sizeof(CalibrationData));
    memset(&pduManager.workingData, 0, sizeof(CalibrationData));
    pduManager.dataSync = true;
}

/* ============================================================
 * Sync Operations
 * ============================================================ */

/**
 * @brief Copy working data --> readWriteData (before flash write)
 */
void syncWorkingToReadWrite(void)
{
    memcpy(&pduManager.readWriteData, &pduManager.workingData,
           sizeof(CalibrationData));
    pduManager.dataSync = true;
}

/**
 * @brief Copy readWriteData --> working data (after flash read or M-Board update)
 */
void syncReadWriteToWorking(void)
{
    memcpy(&pduManager.workingData, &pduManager.readWriteData,
           sizeof(CalibrationData));
    pduManager.dataSync = true;
}

/* ============================================================
 * CRC Operations
 * ============================================================ */

/**
 * @brief Calculate CRC-16 (polynomial 0x8005) over a uint16 array
 * @param data   Pointer to data array
 * @param length Number of 16-bit words
 * @return CRC-16 value
 */
uint16_t calculateCRC(uint16_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    size_t i, j;

    for (i = 0; i < length; i++)
    {
        crc ^= data[i];
        for (j = 0; j < 16; j++)
        {
            if (crc & 0x8000)
            {
                crc = (crc << 1) ^ CRC16_POLYNOMIAL;
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Compute CRC over pduData and store in calData->crc
 * @param calData Pointer to CalibrationData to update
 */
void computeFlashCRC(CalibrationData *calData)
{
    calData->crc = calculateCRC(
        (uint16_t *)&calData->pduData,
        sizeof(PDUData) / sizeof(uint16_t)
    );
}

/**
 * @brief Validate CRC in receivedBuffer (wire format from M-Board)
 *
 * Checks receivedBuffer[0..WIRE_DATA_SIZE-1] against the CRC at
 * receivedBuffer[WIRE_CRC_OFFSET].
 *
 * @return true if CRC matches, false otherwise
 */
bool validateWireCRC(void)
{
    uint16_t computed = calculateCRC(receivedBuffer, WIRE_DATA_SIZE);
    uint16_t received = receivedBuffer[WIRE_CRC_OFFSET];
    return (computed == received);
}

/**
 * @brief Validate CRC in a CalibrationData record (from flash)
 *
 * NOTE: Old firmware wrote flash without computing CRC (crc field = 0).
 * A CRC of 0 with valid-looking data is treated as "unverified but usable"
 * by the caller — validateCalibrationGains() clamps bad values.
 *
 * @param calData Pointer to CalibrationData to validate
 * @return true if CRC matches, false otherwise
 */
bool validateFlashCRC(const CalibrationData *calData)
{
    uint16_t computed = calculateCRC(
        (uint16_t *)&calData->pduData,
        sizeof(PDUData) / sizeof(uint16_t)
    );
    return (computed == calData->crc);
}

/* ============================================================
 * Gain Validation
 * ============================================================ */

/**
 * @brief Clamp a single gain value to the valid range [GAIN_MIN..GAIN_MAX].
 * @param gain Pointer to the gain value to validate
 */
static void clampGain(uint16_t *gain)
{
    if (*gain < GAIN_MIN || *gain > GAIN_MAX)
    {
        *gain = GAIN_DEFAULT;
    }
}

/**
 * @brief Validate and sanitize ALL calibration gains in readWriteData.
 *
 * Checks:
 *   - 216 CT current gains
 *   - 216 kW power gains
 *   - 19 S-board parameters (primary/secondary voltages, currents, kW, ground)
 *
 * Any value outside [GAIN_MIN..GAIN_MAX] is reset to GAIN_DEFAULT (10000 = 1.0x).
 *
 * This is critical for legacy flash data which may contain raw ADC values
 * (e.g., 3770, 3300) in gain positions — those would be misinterpreted as
 * 3.77x/3.3x multipliers and cause overflow.
 */
void validateCalibrationGains(void)
{
    PDUData *pd = &pduManager.readWriteData.pduData;
    int i;

    /* Validate CT and kW gains (432 values) */
    for (i = 0; i < numberOfCTs; i++)
    {
        clampGain(&pd->gains.ctGain[i]);
        clampGain(&pd->gains.kwGain[i]);
    }

    /* Validate the 19 S-board calibration parameters */
    clampGain(&pd->primary.voltage.R);
    clampGain(&pd->primary.voltage.Y);
    clampGain(&pd->primary.voltage.B);
    clampGain(&pd->primary.current.R);
    clampGain(&pd->primary.current.Y);
    clampGain(&pd->primary.current.B);
    clampGain(&pd->primary.neutralCurrent);
    clampGain(&pd->primary.totalKW);

    clampGain(&pd->secondary.voltage.R);
    clampGain(&pd->secondary.voltage.Y);
    clampGain(&pd->secondary.voltage.B);
    clampGain(&pd->secondary.current.R);
    clampGain(&pd->secondary.current.Y);
    clampGain(&pd->secondary.current.B);
    clampGain(&pd->secondary.neutralCurrent);
    clampGain(&pd->secondary.kw.R);
    clampGain(&pd->secondary.kw.Y);
    clampGain(&pd->secondary.kw.B);

    clampGain(&pd->groundCurrent);
}

/* ============================================================
 * Flash Helpers
 * ============================================================ */

/**
 * @brief Check if sector index has hit the wrap limit and erase Bank 4 if so.
 *
 * When newestData >= WRAP_LIMIT (0xFFFE), all sectors are used.
 * Erase the entire bank and reset counters to start fresh.
 *
 * @return true if bank was erased, false if no action needed
 */
bool checkFlashWrapAround(void)
{
    if (newestData >= WRAP_LIMIT)
    {
        eraseBank4();
        newestData = 0;
        oldestData = 0xFFFF;
        writeAddress = 0xFFFFFFFF;
        addressToWrite = BANK4_START;
        return true;
    }
    return false;
}

/**
 * @brief Copy readWriteData to the flash programming Buffer and set datasize.
 *
 * Call this AFTER populating readWriteData (including CRC), then call
 * Example_ProgramUsingAutoECC() to commit to flash.
 */
void prepareForFlashWrite(void)
{
    memcpy(Buffer, &pduManager.readWriteData, sizeof(CalibrationData));
    datasize = sizeof(CalibrationData) / sizeof(uint16_t);
}

/* ============================================================
 * Write Default Calibration Values
 * ============================================================ */

/**
 * @brief Populate readWriteData with factory-default gains (all 10000 = 1.0x)
 *        and prepare for flash write.
 *
 * After calling this, the caller must invoke Example_ProgramUsingAutoECC()
 * to actually write to flash.
 */
void writeDefaultCalibrationValues(void)
{
    int i;

    checkFlashWrapAround();

    /* Build default PDUData — all gains set to GAIN_DEFAULT */
    PDUData defaultPduData = {
        .primary = {
            .voltage = { .R = GAIN_DEFAULT, .Y = GAIN_DEFAULT, .B = GAIN_DEFAULT },
            .current = { .R = GAIN_DEFAULT, .Y = GAIN_DEFAULT, .B = GAIN_DEFAULT },
            .neutralCurrent = GAIN_DEFAULT,
            .totalKW = GAIN_DEFAULT
        },
        .secondary = {
            .voltage = { .R = GAIN_DEFAULT, .Y = GAIN_DEFAULT, .B = GAIN_DEFAULT },
            .current = { .R = GAIN_DEFAULT, .Y = GAIN_DEFAULT, .B = GAIN_DEFAULT },
            .neutralCurrent = GAIN_DEFAULT,
            .kw = { .R = GAIN_DEFAULT, .Y = GAIN_DEFAULT, .B = GAIN_DEFAULT }
        },
        .groundCurrent = GAIN_DEFAULT
    };

    for (i = 0; i < numberOfCTs; i++)
    {
        defaultPduData.gains.ctGain[i] = GAIN_DEFAULT;
        defaultPduData.gains.kwGain[i] = GAIN_DEFAULT;
    }

    /* Build CalibrationData record */
    pduManager.readWriteData.sectorIndex = ++newestData;
    memcpy(&pduManager.readWriteData.pduData, &defaultPduData, sizeof(PDUData));

    /* Compute and store CRC over pduData */
    computeFlashCRC(&pduManager.readWriteData);

    /* Prepare Buffer for flash programming */
    prepareForFlashWrite();

    pduManager.dataSync = false;
}

/* ============================================================
 * M-Board Receive Path
 * ============================================================ */

/**
 * @brief Parse receivedBuffer (wire format from M-Board) into
 *        pduManager.readWriteData, compute CRC, and prepare for flash write.
 *
 * Wire-format buffer layout (set by copyRxFIFO):
 *   [0..215]     ctGain[0..215]
 *   [216..431]   kwGain[0..215]
 *   [432..450]   19 S-board parameters (interleaved pri/sec)
 *   [451]        CRC16
 *
 * @return true if wire CRC is valid and data was parsed successfully,
 *         false if CRC mismatch (readWriteData not modified)
 */
bool RxBufferToStruct(void)
{
    int i;

    /* Step 1: Validate wire CRC from M-Board */
    if (!validateWireCRC())
    {
        return false;
    }

    /* Step 2: Handle flash sector wrap-around */
    checkFlashWrapAround();

    /* Step 3: Parse CT and kW gains (direct 1:1 mapping, same order in wire and struct) */
    for (i = 0; i < numberOfCTs; i++)
    {
        pduManager.readWriteData.pduData.gains.ctGain[i] = receivedBuffer[i];
        pduManager.readWriteData.pduData.gains.kwGain[i] = receivedBuffer[numberOfCTs + i];
    }

    /* Step 4: Parse 19 S-board parameters (wire order != struct order) */

    /* Primary voltages */
    pduManager.readWriteData.pduData.primary.voltage.R = receivedBuffer[WIRE_PRI_VOLT_R];
    pduManager.readWriteData.pduData.primary.voltage.Y = receivedBuffer[WIRE_PRI_VOLT_Y];
    pduManager.readWriteData.pduData.primary.voltage.B = receivedBuffer[WIRE_PRI_VOLT_B];

    /* Secondary voltages */
    pduManager.readWriteData.pduData.secondary.voltage.R = receivedBuffer[WIRE_SEC_VOLT_R];
    pduManager.readWriteData.pduData.secondary.voltage.Y = receivedBuffer[WIRE_SEC_VOLT_Y];
    pduManager.readWriteData.pduData.secondary.voltage.B = receivedBuffer[WIRE_SEC_VOLT_B];

    /* Primary currents */
    pduManager.readWriteData.pduData.primary.current.R = receivedBuffer[WIRE_PRI_CURR_R];
    pduManager.readWriteData.pduData.primary.current.Y = receivedBuffer[WIRE_PRI_CURR_Y];
    pduManager.readWriteData.pduData.primary.current.B = receivedBuffer[WIRE_PRI_CURR_B];

    /* Secondary currents */
    pduManager.readWriteData.pduData.secondary.current.R = receivedBuffer[WIRE_SEC_CURR_R];
    pduManager.readWriteData.pduData.secondary.current.Y = receivedBuffer[WIRE_SEC_CURR_Y];
    pduManager.readWriteData.pduData.secondary.current.B = receivedBuffer[WIRE_SEC_CURR_B];

    /* Neutral currents */
    pduManager.readWriteData.pduData.primary.neutralCurrent = receivedBuffer[WIRE_PRI_NEUTRAL];
    pduManager.readWriteData.pduData.secondary.neutralCurrent = receivedBuffer[WIRE_SEC_NEUTRAL];

    /* Power */
    pduManager.readWriteData.pduData.primary.totalKW = receivedBuffer[WIRE_PRI_TOTAL_KW];
    pduManager.readWriteData.pduData.secondary.kw.R = receivedBuffer[WIRE_SEC_KW_R];
    pduManager.readWriteData.pduData.secondary.kw.Y = receivedBuffer[WIRE_SEC_KW_Y];
    pduManager.readWriteData.pduData.secondary.kw.B = receivedBuffer[WIRE_SEC_KW_B];

    /* Ground current */
    pduManager.readWriteData.pduData.groundCurrent = receivedBuffer[WIRE_GROUND_CURR];

    /* Step 5: Assign sector index */
    pduManager.readWriteData.sectorIndex = ++newestData;

    /* Step 6: Compute CRC over the struct-layout pduData for flash storage */
    computeFlashCRC(&pduManager.readWriteData);

    /* Step 7: Prepare Buffer for flash programming */
    prepareForFlashWrite();

    pduManager.dataSync = false;
    return true;
}

/* ============================================================
 * Struct-to-Wire Conversion (for M-Board responses)
 * ============================================================ */

/**
 * @brief Convert struct-layout PDUData back to wire-format buffer.
 *
 * This reverses the mapping done by RxBufferToStruct(), producing a
 * buffer in the same format that M-Board originally sent.  Useful for
 * sendCalibrationDataToMBoard() so the round-trip is lossless.
 *
 * @param pduData    Source struct-layout data
 * @param wireBuffer Destination buffer (must hold at least WIRE_DATA_SIZE words)
 */
void structToWireBuffer(const PDUData *pduData, uint16_t *wireBuffer)
{
    int i;

    /* CT and kW gains — same order in wire and struct */
    for (i = 0; i < numberOfCTs; i++)
    {
        wireBuffer[i] = pduData->gains.ctGain[i];
        wireBuffer[numberOfCTs + i] = pduData->gains.kwGain[i];
    }

    /* 19 S-board parameters in wire order (interleaved pri/sec) */
    wireBuffer[WIRE_PRI_VOLT_R]   = pduData->primary.voltage.R;
    wireBuffer[WIRE_PRI_VOLT_Y]   = pduData->primary.voltage.Y;
    wireBuffer[WIRE_PRI_VOLT_B]   = pduData->primary.voltage.B;

    wireBuffer[WIRE_SEC_VOLT_R]   = pduData->secondary.voltage.R;
    wireBuffer[WIRE_SEC_VOLT_Y]   = pduData->secondary.voltage.Y;
    wireBuffer[WIRE_SEC_VOLT_B]   = pduData->secondary.voltage.B;

    wireBuffer[WIRE_PRI_CURR_R]   = pduData->primary.current.R;
    wireBuffer[WIRE_PRI_CURR_Y]   = pduData->primary.current.Y;
    wireBuffer[WIRE_PRI_CURR_B]   = pduData->primary.current.B;

    wireBuffer[WIRE_SEC_CURR_R]   = pduData->secondary.current.R;
    wireBuffer[WIRE_SEC_CURR_Y]   = pduData->secondary.current.Y;
    wireBuffer[WIRE_SEC_CURR_B]   = pduData->secondary.current.B;

    wireBuffer[WIRE_PRI_NEUTRAL]  = pduData->primary.neutralCurrent;
    wireBuffer[WIRE_SEC_NEUTRAL]  = pduData->secondary.neutralCurrent;

    wireBuffer[WIRE_PRI_TOTAL_KW] = pduData->primary.totalKW;
    wireBuffer[WIRE_SEC_KW_R]     = pduData->secondary.kw.R;
    wireBuffer[WIRE_SEC_KW_Y]     = pduData->secondary.kw.Y;
    wireBuffer[WIRE_SEC_KW_B]     = pduData->secondary.kw.B;

    wireBuffer[WIRE_GROUND_CURR]  = pduData->groundCurrent;
}

/* ============================================================
 * CAN Frame Preparation
 * ============================================================ */

/**
 * @brief Pack a uint16 buffer into CAN-FD frames for M-Board transmission.
 *
 * Each frame: 2-byte order header + up to 62 bytes (31 uint16 words) payload.
 * Frames are stored in storedFrames[] for later transmission/retransmission.
 *
 * @param buffer     Source data (uint16 array)
 * @param bufferSize Number of uint16 words to pack
 */
void prepareFramesForTransmission(uint16_t *buffer, uint16_t bufferSize)
{
    uint32_t remainingWords = bufferSize;
    uint32_t bufferOffset = 0;
    uint32_t j;
    uint16_t i, value;

    for (i = 0; i < NUM_FRAMES && remainingWords > 0; i++)
    {
        CANFrame *frame = &storedFrames[i];
        frame->order = i + 1;

        /* 2-byte header: frame order (big-endian) */
        frame->data[0] = (frame->order >> 8) & 0xFF;
        frame->data[1] = frame->order & 0xFF;

        /* Payload: up to 31 uint16 words = 62 bytes */
        uint32_t wordsToSend = (remainingWords < 31) ? remainingWords : 31;
        for (j = 0; j < wordsToSend; j++)
        {
            value = buffer[bufferOffset + j];
            frame->data[2 + j * 2]     = (value >> 8) & 0xFF;  /* High byte */
            frame->data[2 + j * 2 + 1] = value & 0xFF;         /* Low byte  */
        }

        /* Zero-fill remaining frame bytes */
        for (j = wordsToSend; j < 31; j++)
        {
            frame->data[2 + j * 2]     = 0;
            frame->data[2 + j * 2 + 1] = 0;
        }

        bufferOffset += wordsToSend;
        remainingWords -= wordsToSend;
    }
}
