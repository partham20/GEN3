/**
 * @file m_board.c
 * @brief M-board communication management implementation
 */

#include "m_board.h"
#include "bu_board.h"
#include "can_driver.h"
#include "pdu_manager.h"
#include "flash_driver.h"

// Global variables
bool CommandReceived = false;
bool versionNumberRequested = false;
bool startupReceive = false;
bool calibStart = false;
uint32_t mBoardCommandID = 0;

volatile bool calibrationInProgress = false;
bool calibDataTransmissionInProgress = false;
bool calibDataTransmissionComplete = false;
uint8_t transmittedFrameCount = 0;
uint32_t lastTransmissionTime = 0;
uint8_t transmissionRetryCount = 0;
bool waitingForAck = false;

/**
 * @brief Send discovery start acknowledgement to M-board
 * Called immediately when CMD_START_DISCOVERY is received from M-board.
 */
void sendDiscoveryStartAck(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id = ((uint32_t)(0x4)) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    TxMsg.data[0] = CMD_START_DISCOVERY;  // 0x05 - echo command
    TxMsg.data[1] = STATUS_OK;            // 0x01 - acknowledged

    MCAN_writeMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(MCANB_DRIVER_BASE, 0U);

    canTxWaitComplete(MCANB_DRIVER_BASE);
}

/**
 * @brief Send stop calibration acknowledgement to M-board
 */
void sendStopCalibrationAck(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id = ((uint32_t)(0x4)) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    TxMsg.data[0] = CMD_STOP_CALIBRATION;  // 0x06 - echo command
    TxMsg.data[1] = STATUS_OK;             // 0x01 - acknowledged

    MCAN_writeMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(MCANB_DRIVER_BASE, 0U);

    canTxWaitComplete(MCANB_DRIVER_BASE);
}

/**
 * @brief Send calibration data format version to M-board
 */
void sendCalibrationDataFormatVersion(void)
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

    uint8_t datare[] = { 0x01, majorVersionNumber, minorVersionNumber, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                         0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    int i;
    for (i = 0; i < 64; i++)
    {
        TxMsg.data[i] = datare[i];
    }

    MCAN_writeMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

    // Add transmission request for Tx buffer 0
    MCAN_txBufAddReq(MCANB_DRIVER_BASE, 0U);

    // Wait for transmission (with timeout)
    canTxWaitComplete(MCANB_DRIVER_BASE);
}

/**
 * @brief Handle retransmission request from M-board
 * @param bitMask Bit mask indicating which frames to retransmit
 */
void handleMBoardRetransmissionRequest(const uint8_t *bitMask)
{
    int i;
    bool needsRetransmission = false;
    uint8_t retransmissionData[15] = {0}; // Array to track which frames need retransmission

    // First byte is command (0x03), start checking from second byte
    for (i = 0; i < 15; i++)
    {
        // If bit is 0, that frame needs retransmission
        if (bitMask[i + 1] == 0x00)
        {
            retransmissionData[i] = 0x00;  // Mark for retransmission
            needsRetransmission = true;
        }
        else
        {
            retransmissionData[i] = 0x01;  // Frame already received
        }
    }

    // Only proceed if there are frames to retransmit
    if (needsRetransmission)
    {
        sendMissingFrames(retransmissionData);
    }
}

/**
 * @brief Send missing frames to M-board
 * @param retransmissionData Bit mask indicating frames to send
 */
void sendMissingFrames(const uint8_t *retransmissionData)
{
    int i;

    // Configure message for transmission
    MCAN_TxBufElement txMsg;

    // Loop through all possible frames
    for (i = 0; i < 15; i++)
    {
        if (retransmissionData[i] == 0x00)
        {  // This frame needs retransmission
            // Wait for any pending transmission to complete (with timeout)
            canTxWaitBufComplete(MCANB_DRIVER_BASE, (1U << i));

            // Clear message structure
            memset(&txMsg, 0, sizeof(MCAN_TxBufElement));

            // Copy data from stored frames
            memcpy(txMsg.data, storedFrames[i].data, 64);

            // Set up CAN message properties for M-Board (ID 0x2)
            txMsg.id = ((uint32_t)(0x2)) << 18U;
            txMsg.dlc = 15U;
            txMsg.fdf = 1U;
            txMsg.brs = 1U;

            // Write message to CAN RAM
            MCAN_writeMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_BUF, i, &txMsg);

            // Request transmission
            MCAN_txBufAddReq(MCANB_DRIVER_BASE, i);

            // Wait for transmission to complete (with timeout)
            canTxWaitBufComplete(MCANB_DRIVER_BASE, (1U << i));
        }
    }
}

/**
 * @brief Handle retransmission request from M-board
 * @param rxMsg Pointer to received CAN message
 */
void handleRetransmissionRequest(const MCAN_RxBufElement *rxMsg)
{
    // Check if this is a retransmission request (command 0x03)
    if (rxMsg->data[0] == 0x03)
    {
        // Pass the bitmask portion of the message
        handleMBoardRetransmissionRequest(rxMsg->data);
    }
}

/**
 * @brief Send calibration data from S-board to M-board
 * This function is called after discovery is complete and BU calibration data is received
 */
void sendCalibrationDataToMBoard(void)
{
    // First, ensure we have all necessary data from BU boards
    if (!buDataCollectionComplete || !buDataProcessed) {
        return;  // Exit if BU data collection is not complete or processed
    }

    // Load the 19 S-board parameters from the latest flash sector.
    // This ensures we send the actual calibration values written by M-Board
    // (not defaults), even if readWriteData was modified in RAM.
    findReadAndWriteAddress();
    if (newestData > 0) {
        const CalibrationData *flashData = (const CalibrationData *)readAddress;
        pduManager.readWriteData.pduData.primary = flashData->pduData.primary;
        pduManager.readWriteData.pduData.secondary = flashData->pduData.secondary;
        pduManager.readWriteData.pduData.groundCurrent = flashData->pduData.groundCurrent;
        validateCalibrationGains();
    }

    syncReadWriteToWorking();

    // Prepare frames for transmission — send only PDUData (skip sectorIndex and crc)
    // BU board gains come from live discovery, S-board params come from latest flash
    prepareFramesForTransmission((uint16_t *)(&pduManager.readWriteData.pduData),
                                sizeof(PDUData) / sizeof(uint16_t));
    // Now transmit all frames to M-board (ID 0x2)
    int i;
    for (i = 0; i < NUM_FRAMES; i++) {
        // Wait for buffer availability (with timeout)
        canTxWaitBufComplete(MCANB_DRIVER_BASE, (1U << i));

        // Configure message for M-board (ID 0x2)
        MCAN_TxBufElement txMsg;
        memset(&txMsg, 0, sizeof(MCAN_TxBufElement));

        // Copy data from stored frames
        memcpy(txMsg.data, storedFrames[i].data, 64);

        // Set up CAN message properties for M-Board (ID 0x2)
        txMsg.id = ((uint32_t)(0x2)) << 18U;
        txMsg.dlc = 15U;  // 64-byte message (DLC=15)
        txMsg.fdf = 1U;   // FD format
        txMsg.brs = 1U;   // Bit rate switching
        txMsg.xtd = 0;    // Standard ID
        txMsg.rtr = 0;    // Not a remote frame
        txMsg.esi = 0U;   // Error state indicator
        txMsg.efc = 1U;   // Event FIFO control
        txMsg.mm = 0xAAU; // Message marker

        // Write message to CAN RAM
        MCAN_writeMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_BUF, i, &txMsg);

        // Request transmission
        MCAN_txBufAddReq(MCANB_DRIVER_BASE, i);

        // Wait for transmission to complete (with timeout)
        canTxWaitBufComplete(MCANB_DRIVER_BASE, (1U << i));

        // Small delay between frames to avoid flooding the bus
        DEVICE_DELAY_US(500);
    }

    // Reset tracking variables after transmission
    sendToMBoard = false;
}
