/**
 * @file bu_board.c
 * @brief BU board discovery and communication management implementation
 */

#include "bu_board.h"
#include "can_driver.h"
#include "pdu_manager.h"
#include "timer_driver.h"
#include "flash_driver.h"
#include <string.h>

// Global variables
BUBoardData buBoards[MAX_BU_BOARDS];
BoardAckStatus boardAckStatus[MAX_BU_BOARDS];
DiscoveredBoard discoveredBoardsInfo[MAX_BU_BOARDS];
uint32_t activeBUMask = 0;
uint32_t discoveryMask = 0;
uint8_t numActiveBUBoards = 0;
bool discoveryInProgress = false;
uint32_t receivedCurrentFrames = 0;
uint32_t receivedKWFrames = 0;
bool duplicateIDFound = false;
uint32_t acknowledgedBoardCount = 0;
bool buMessagePending[MCAN_RX_BUFF_NUM] = {false};
bool buMessageReceived = false;
bool buDataCollectionActive = false;
bool buDataCollectionComplete = false;
bool buDataProcessed = false;

/**
 * @brief Initialize discovery tracking
 */
void initDiscoveryTracking(void)
{
    memset(discoveredBoardsInfo, 0, sizeof(discoveredBoardsInfo));
    acknowledgedBoardCount = 0;
    duplicateIDFound = false;
}

/**
 * @brief Start BU board discovery process
 */
void startBUDiscovery(void)
{
    // Reset discovery variables
    discoveryMask = 0;
    discoveryInProgress = true;

//     Initialize discovery tracking
    initDiscoveryTracking();

    // Send discovery start message
    sendDiscoveryStartMessage();

    // Start 10-second timer
    restart10SecTimer();
}

/**
 * @brief Finalize BU discovery process
 */
void finalizeBUDiscovery(void)
{
    // Set flag to indicate discovery is no longer in progress
    discoveryInProgress = false;

    // Update activeBUMask with newly discovered boards
    activeBUMask |= discoveryMask;

    // Update board status while preserving existing active boards
    uint32_t mask = activeBUMask;
    uint32_t index = 0;
    while (mask) {
        if (mask & 1) {
            buBoards[index].isActive = true;  // Set or maintain active status
        }
        mask >>= 1;
        index++;
    }

    // Update count of active boards based on current mask
    numActiveBUBoards = 0;
    mask = activeBUMask;
    while (mask) {
        if (mask & 1) {
            numActiveBUBoards++;
        }
        mask >>= 1;
    }

    // Send discovery stop message
    sendDiscoveryStopMessage();

    // Stop timer
    stop10SecTimer();
}

/**
 * @brief Start a new discovery process
 */
void startNewDiscovery(void)
{
    // Stop any ongoing timers
    stop10SecTimer();

    // Reset discovery flags
    discoveryInProgress = false;
    discoveryMask = 0;

    // This is the ONLY place where we reset activeBUMask
    activeBUMask = 0;
    numActiveBUBoards = 0;

    // Reset calibration state
    buDataCollectionActive = false;
    buDataCollectionComplete = false;
    buDataProcessed = false;

    // Reset frame tracking
    receivedCurrentFrames = 0;
    receivedKWFrames = 0;

    // Clear all BU board data
    memset(buBoards, 0, sizeof(buBoards));

    // Start fresh discovery
    startBUDiscovery();
}

/**
 * @brief Send discovery start message
 */
void sendDiscoveryStartMessage(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    // Configure message properties
    TxMsg.id = ((uint32_t)(0x4)) << 18U;  // Using ID 4
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;  // 64 byte message
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    // Prepare 64-byte data array with discovery start command
    uint8_t data[64] = {
        DISCOVERY_START_CMD,   // Command byte
        STATUS_OK,            // Status byte
        0x00                  // Rest zeroed out
    };

    // Copy data to message
    memcpy(TxMsg.data, data, 64);

    // Send message
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);

    // Wait for transmission (with timeout)
    canTxWaitComplete(CAN_BU_BASE);
}

/**
 * @brief Send discovery stop message
 */
void sendDiscoveryStopMessage(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    // Configure message properties
    TxMsg.id = ((uint32_t)(0x4)) << 18U;  // Using ID 4
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;  // 64 byte message
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    // Prepare 64-byte data array with discovery stop command
    uint8_t data[64] = {
        DISCOVERY_STOP_CMD,    // Command byte
        STATUS_OK,            // Status byte
        0x00                  // Rest zeroed out
    };

    // Copy data to message
    memcpy(TxMsg.data, data, 64);

    // Send message
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);

    // Wait for transmission (with timeout)
    canTxWaitComplete(CAN_BU_BASE);
}

/**
 * @brief Check if a board has already been acknowledged
 * @param boardID ID of the board to check
 * @return true if board already acknowledged, false otherwise
 */
bool isBoardAcknowledged(uint32_t boardID)
{
    if (boardID < FIRST_BU_ID || boardID >= (FIRST_BU_ID + MAX_BU_BOARDS)) {
        return false;
    }

    uint32_t boardIndex = boardID - FIRST_BU_ID;
    return discoveredBoardsInfo[boardIndex].acknowledged;
}

/**
 * @brief Record board acknowledgment
 * @param boardID ID of the board to acknowledge
 */
void recordBoardAcknowledgment(uint32_t boardID)
{
    if (boardID < FIRST_BU_ID || boardID >= (FIRST_BU_ID + MAX_BU_BOARDS)) {
        return;
    }

    uint32_t boardIndex = boardID - FIRST_BU_ID;
    if (!discoveredBoardsInfo[boardIndex].acknowledged) {
        discoveredBoardsInfo[boardIndex].boardId = boardID;
        discoveredBoardsInfo[boardIndex].acknowledged = true;
        acknowledgedBoardCount++;
    }
}

/**
 * @brief Process discovery response from BU board
 * @param boardID ID of the responding board
 */
void processBUDiscoveryResponse(uint32_t boardID)
{
    if (!discoveryInProgress) {
        return;
    }

    if (boardID >= FIRST_BU_ID && boardID < (FIRST_BU_ID + MAX_BU_BOARDS)) {
        uint32_t boardIndex = boardID - FIRST_BU_ID;

        // Check if this board has already been acknowledged
        if (isBoardAcknowledged(boardID)) {
            // This is a duplicate response from an already acknowledged board
            duplicateIDFound = true;
           // sendDuplicateIDError(boardID);
            return;
        }

        // Record the discovered board
        discoveryMask |= (1UL << boardIndex);
        recordBoardAcknowledgment(boardID);

        // Send acknowledgment for this board's discovery response
        sendDiscoveryResponseAck(boardID);
    }
}

/**
 * @brief Send acknowledgment for discovery response
 * @param boardId ID of the board to acknowledge
 */
void sendDiscoveryResponseAck(uint32_t boardId)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    // Configure message properties
    TxMsg.id = ((uint32_t)(0x4)) << 18U;  // Using ID 4 for acknowledgments
    TxMsg.brs = 0x1;                       // Enable bit rate switching
    TxMsg.dlc = 15;                        // 64 byte message
    TxMsg.rtr = 0;                         // Not a remote frame
    TxMsg.xtd = 0;                         // Standard ID
    TxMsg.fdf = 0x1;                       // FD format
    TxMsg.esi = 0U;                        // Error state indicator
    TxMsg.efc = 1U;                        // Event FIFO control
    TxMsg.mm = 0xAAU;                      // Message marker

    // Prepare acknowledgment data
    uint8_t data[64] = { 0 };              // Initialize all bytes to 0
    data[0] = 101 + (boardId - FIRST_BU_ID);  // 101 + boardIndex (starts from 0)
    data[1] = 1;                              // Discovery ACK
    data[2] = 1;                              // Discovery ACK

    // Copy data to message
    memcpy(TxMsg.data, data, 64);

    // Write message to message RAM
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);

    // Request transmission
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);

    // Wait for transmission to complete (with timeout)
    canTxWaitComplete(CAN_BU_BASE);
}

/**
 * @brief Send error message for duplicate board ID
 * @param boardID ID of the duplicate board
 */
void sendDuplicateIDError(uint32_t boardID)
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

    // Error message format: [0x0F (error command), boardID]
    uint8_t data[64] = {
        0x0F,                    // Error command
        (uint8_t)boardID,        // Duplicate board ID
    };

    memcpy(TxMsg.data, data, 64);
    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

    canTxWaitComplete(CAN_MBOARD_BASE);
}

/**
 * @brief Initialize BU tracking structures
 */
void initBUTracking(void)
{
    memset(buBoards, 0, sizeof(buBoards));
    activeBUMask = 0;
    discoveryMask = 0;
    numActiveBUBoards = 0;
    receivedCurrentFrames = 0;
    receivedKWFrames = 0;
    allFramesReceived = false;
    discoveryInProgress = false;
}

/**
 * @brief Start new BU data collection cycle
 */
void startNewBUDataCollection(void)
{
    receivedCurrentFrames = 0;
    receivedKWFrames = 0;
    buDataCollectionComplete = false;
    buDataProcessed = false;
}

/**
 * @brief Update received buffer with board data
 * @param boardIndex Index of the board in array
 * @param frameType Type of frame (current or kW)
 */
void updateReceivedBuffer(uint32_t boardIndex, uint8_t frameType)
{
    // First check if this is an active board
    if (!(activeBUMask & (1UL << boardIndex)))
    {
        return;  // Exit if board is not active
    }

    if (frameType == BU_RESPONSE_FRAME_TYPE)
    {  // Current gains
        // Calculate starting position in receivedBuffer
        // Only count active boards before this one to determine offset
        uint32_t activeBoards = 0;
        uint32_t mask = activeBUMask & ((1UL << boardIndex) - 1);
        while (mask)
        {
            activeBoards += mask & 1;
            mask >>= 1;
        }
        uint32_t startPos = activeBoards * CTS_PER_BOARD;

        // Copy current gains for this board
        memcpy(&receivedBuffer[startPos], buBoards[boardIndex].currentGains,
              CTS_PER_BOARD * sizeof(uint16_t));
    }
    else if (frameType == BU_KW_FRAME_TYPE)
    {  // kW gains
        // Calculate starting position for kW gains
        // First skip all current gains (numActiveBUBoards * CTS_PER_BOARD)
        // Then find position for this board's kW gains
        uint32_t activeBoards = 0;
        uint32_t mask = activeBUMask & ((1UL << boardIndex) - 1);
        while (mask)
        {
            activeBoards += mask & 1;
            mask >>= 1;
        }
        uint32_t startPos = (numActiveBUBoards * CTS_PER_BOARD)
                + (activeBoards * CTS_PER_BOARD);

        // Copy kW gains for this board
        memcpy(&receivedBuffer[startPos], buBoards[boardIndex].kwGains,
              CTS_PER_BOARD * sizeof(uint16_t));
    }
}

/**
 * @brief Send acknowledgment for BU frame
 * @param boardId ID of the board to acknowledge
 * @param currentGainReceived Flag indicating current gain received
 * @param kwGainReceived Flag indicating kW gain received
 */
void sendBUFrameAck(uint32_t boardId, bool currentGainReceived, bool kwGainReceived)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    // Configure message properties
    TxMsg.id = ((uint32_t)(0x4)) << 18U;  // Using ID 4 as per existing code
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;  // 64 byte message
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    // Prepare acknowledgment data
    uint8_t data[64] = { 0 };
    data[0] = 101 + (boardId - FIRST_BU_ID);  // 101 + boardIndex (starts from 0)
    data[1] = currentGainReceived ? 1 : 0;    // Current gain status
    data[2] = kwGainReceived ? 1 : 0;         // KW gain status

    // Copy data to message
    memcpy(TxMsg.data, data, 64);

    // Send message
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);

    // Wait for transmission (with timeout)
    canTxWaitComplete(CAN_BU_BASE);
}

/**
 * @brief Process BU messages from CAN buffer
 */
void processBUMessages(void)
{
    int i;
    for (i = 0; i < MCAN_RX_BUFF_NUM; i++)
    {
        if (buMessagePending[i])
        {
            uint32_t boardId = (rxMsg[i].id >> 18U) & 0x1F;
            uint8_t frameType = rxMsg[i].data[1];

            if (discoveryInProgress)
            {
                processBUDiscoveryResponse(boardId);
            }
            else if (frameType == BU_RESPONSE_FRAME_TYPE ||
                     frameType == BU_KW_FRAME_TYPE)
            {
                processBUFrameInISR(&rxMsg[i]);
            }
            buMessagePending[i] = false;
        }
    }
}

/**
 * @brief Process BU frame in interrupt context
 * @param rxMsg Pointer to received CAN message
 */
void processBUFrameInISR(const MCAN_RxBufElement *rxMsg)
{
    uint32_t boardID = (rxMsg->id >> 18U) & 0x1F;
    uint32_t boardIndex = boardID - FIRST_BU_ID;
    uint8_t frameType = rxMsg->data[1];

    // Validate board ID and active status
    if (boardIndex >= MAX_BU_BOARDS || !(activeBUMask & (1UL << boardIndex)))
    {
        return;
    }

    bool currentReceived = buBoards[boardIndex].currentFrameReceived;
    bool kwReceived = buBoards[boardIndex].kwFrameReceived;

    if (frameType == BU_RESPONSE_FRAME_TYPE)
    {
        // Process current gains
        int i;
        for (i = 0; i < CTS_PER_BOARD; i++)
        {
            buBoards[boardIndex].currentGains[i] = (rxMsg->data[2 + i * 2] << 8) |
                                                  rxMsg->data[3 + i * 2];
        }
        buBoards[boardIndex].currentFrameReceived = true;
        receivedCurrentFrames |= (1UL << boardIndex);
        currentReceived = true;
        updateReceivedBuffer(boardIndex, frameType);
    }
    else if (frameType == BU_KW_FRAME_TYPE)
    {
        // Process kW gains
        int i;
        for (i = 0; i < CTS_PER_BOARD; i++)
        {
            buBoards[boardIndex].kwGains[i] = (rxMsg->data[2 + i * 2] << 8) |
                                             rxMsg->data[3 + i * 2];
        }
        buBoards[boardIndex].kwFrameReceived = true;
        receivedKWFrames |= (1UL << boardIndex);
        kwReceived = true;
        updateReceivedBuffer(boardIndex, frameType);
    }

    // Send acknowledgment for each received frame
    sendBUFrameAck(boardID, currentReceived, kwReceived);
}

/**
 * @brief Check if all frames have been received
 * @return true if all frames received, false otherwise
 */
bool checkAllFramesReceived(void)
{
    return (receivedCurrentFrames == activeBUMask) &&
           (receivedKWFrames == activeBUMask);
}

/**
 * @brief Get missing frames based on mask
 * @param missingCurrent Pointer to store missing current frames mask
 * @param missingKW Pointer to store missing kW frames mask
 */
void getMissingFrames(uint32_t *missingCurrent, uint32_t *missingKW)
{
    *missingCurrent = activeBUMask & ~receivedCurrentFrames;
    *missingKW = activeBUMask & ~receivedKWFrames;
}

/**
 * @brief Get number of active BU boards
 * @return Number of active boards
 */
uint8_t getNumActiveBUBoards(void)
{
    return numActiveBUBoards;
}

/**
 * @brief Check if a specific board is active
 * @param boardID ID of the board to check
 * @return true if board is active, false otherwise
 */
bool isBoardActive(uint32_t boardID)
{
    if (boardID < FIRST_BU_ID || boardID >= (FIRST_BU_ID + MAX_BU_BOARDS))
    {
        return false;
    }
    uint32_t boardIndex = boardID - FIRST_BU_ID;
    return (activeBUMask & (1UL << boardIndex)) != 0;
}

/**
 * @brief Synchronize BU board data
 */
void syncBUBoardsData(void)
{
    // Update gains in-place, preserving the 19 S-board parameters already in readWriteData
    copyBUBoardsToWorkingStructure(&pduManager.readWriteData.pduData);

    pduManager.readWriteData.sectorIndex = ++newestData;

    // Mark structures as out of sync
    pduManager.dataSync = false;

    buDataProcessed = true;
}

/**
 * @brief Copy BU board data to working structure
 * @param pduData Pointer to PDU data structure to fill
 */
void copyBUBoardsToWorkingStructure(PDUData *pduData)
{
    const uint16_t DEFAULT_CURRENT_GAIN = 10000;
    const uint16_t DEFAULT_KW_GAIN = 10000;

    // Initialize gains arrays
    int totalCTIndex = 0;
    int boardIndex;

    // Copy gains from each BU board (1-12), in order
    for (boardIndex = 0; boardIndex < MAX_BU_BOARDS; boardIndex++)
    {
        int ctIndex;

        if (buBoards[boardIndex].isActive)
        {
            // Copy current gains
            for (ctIndex = 0; ctIndex < CTS_PER_BOARD; ctIndex++)
            {
                pduData->gains.ctGain[totalCTIndex] =
                        buBoards[boardIndex].currentGains[ctIndex];
                pduData->gains.kwGain[totalCTIndex] =
                        buBoards[boardIndex].kwGains[ctIndex];
                totalCTIndex++;
            }
        }
        else
        {
            // Fill with default values for inactive/missing boards
            for (ctIndex = 0; ctIndex < CTS_PER_BOARD; ctIndex++)
            {
                pduData->gains.ctGain[totalCTIndex] = DEFAULT_CURRENT_GAIN;
                pduData->gains.kwGain[totalCTIndex] = DEFAULT_KW_GAIN;
                totalCTIndex++;
            }
        }
    }

    // S-board parameters (primary, secondary, groundCurrent) are NOT touched here.
    // They are preserved from flash-loaded readWriteData, which holds the latest
    // calibration values written by M-Board.
}

/**
 * @brief Copy flash data to BU boards
 */
void copyFlashToBUBoards(void)
{
    // Return if no active boards
    if (activeBUMask == 0) {
        return;
    }

    // First ensure we have valid data in PDU manager
    if (!pduManager.dataSync) {
        syncReadWriteToWorking();
    }

    uint16_t ctIndex = 0;
    int boardIndex;

    // Process each potential BU board
    for (boardIndex = 0; boardIndex < MAX_BU_BOARDS; boardIndex++)
    {
        // Check if this board is active
        if (activeBUMask & (1UL << boardIndex))
        {
            int ctPerBoard;
            // Copy CT gains for this board
            for (ctPerBoard = 0; ctPerBoard < CTS_PER_BOARD && ctIndex < numberOfCTs; ctPerBoard++)
            {
                // Copy current gains
                buBoards[boardIndex].currentGains[ctPerBoard] =
                    pduManager.workingData.pduData.gains.ctGain[ctIndex];

                // Copy kW gains
                buBoards[boardIndex].kwGains[ctPerBoard] =
                    pduManager.workingData.pduData.gains.kwGain[ctIndex];

                ctIndex++;
            }

            // Mark the board as having received both types of data
            buBoards[boardIndex].currentFrameReceived = true;
            buBoards[boardIndex].kwFrameReceived = true;

            // Update received frames masks
            receivedCurrentFrames |= (1UL << boardIndex);
            receivedKWFrames |= (1UL << boardIndex);
        }
    }

    // Verify all active boards have been updated
    if ((receivedCurrentFrames == activeBUMask) &&
        (receivedKWFrames == activeBUMask)) {
        allFramesReceived = true;
    }
}

/**
 * @brief Send stop calibration command to all BU boards via MCANA (CAN ID 4)
 * BU boards will reset themselves upon receiving this.
 */
void sendStopCalibrationToBUBoards(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id = ((uint32_t)(0x4)) << 18U;  // CAN ID 4
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    TxMsg.data[0] = CMD_STOP_CALIBRATION;  // 0x06
    TxMsg.data[1] = STATUS_OK;             // 0x01

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);

    canTxWaitComplete(CAN_BU_BASE);
}

/**
 * @brief Relay an M-Board command to all BU boards via MCANA (CAN ID 4)
 * Forwards the received message data as-is, only changing the CAN ID to 4.
 * @param rxMsg Pointer to the received M-Board message
 */
void relayCommandToBUBoards(const MCAN_RxBufElement *rxMsg)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id = ((uint32_t)(0x4)) << 18U;  // CAN ID 4
    TxMsg.brs = 0x1;
    TxMsg.dlc = rxMsg->dlc;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    int i;
    for (i = 0; i < 64; i++)
    {
        TxMsg.data[i] = rxMsg->data[i];
    }

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);

    canTxWaitComplete(CAN_BU_BASE);
}

/**
 * @brief Send calibration data to specific board
 * @param boardIndex Index of the board in array
 */
void sendBoardCalibrationData(uint32_t boardIndex)
{
    if (!(activeBUMask & (1UL << boardIndex)) || boardIndex >= MAX_BU_BOARDS)
    {
        return; // Exit if board is not active or index is invalid
    }

    MCAN_TxBufElement TxMsg;
    uint32_t canId = boardIndex + FIRST_BU_ID;  // Calculate CAN ID for this board
    uint32_t i;
    uint8_t firstByte = 101 + boardIndex;  // Calculate first byte as 101 + board index

    // Frame 1: Current Gains
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
    TxMsg.id = ((uint32_t)(canId)) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;  // 64-byte message
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    // Command for current gains with new first byte
    TxMsg.data[0] = firstByte;  // First byte is now 101 + board index
    TxMsg.data[1] = 0x01;      // Frame type: Current gains

    // Copy current gains data - assuming 16-bit values packed into bytes
    for (i = 0; i < CTS_PER_BOARD; i++)
    {
        uint16_t gainValue = buBoards[boardIndex].currentGains[i];
        TxMsg.data[2 + (i * 2)] = (gainValue >> 8) & 0xFF;    // High byte
        TxMsg.data[3 + (i * 2)] = gainValue & 0xFF;           // Low byte
    }

    // Send current gains frame
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete(CAN_BU_BASE);

    DEVICE_DELAY_US(3000);  // 3ms delay

    // Frame 2: kW Gains
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
    TxMsg.id = ((uint32_t)(canId)) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    // Command for kW gains with new first byte
    TxMsg.data[0] = firstByte;  // First byte is now 101 + board index
    TxMsg.data[1] = 0x02;      // Frame type: kW gains

    // Copy kW gains data
    for (i = 0; i < CTS_PER_BOARD; i++)
    {
        uint16_t gainValue = buBoards[boardIndex].kwGains[i];
        TxMsg.data[2 + (i * 2)] = (gainValue >> 8) & 0xFF;    // High byte
        TxMsg.data[3 + (i * 2)] = gainValue & 0xFF;           // Low byte
    }

    // Send kW gains frame
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete(CAN_BU_BASE);
}

/**
 * @brief Send calibration data to all active boards
 */
void sendAllBoardsCalibrationData(void)
{
    uint32_t boardIndex;

    for (boardIndex = 0; boardIndex < MAX_BU_BOARDS; boardIndex++)
    {
        if (activeBUMask & (1UL << boardIndex))
        {
            sendBoardCalibrationData(boardIndex);

            // Add a small delay between board transmissions
            DEVICE_DELAY_US(1000);  // 1ms delay
        }
    }
}

/**
 * @brief Process BU board acknowledgment
 * @param boardId ID of the board
 * @param currentGainAck Current gain acknowledgment flag
 * @param kwGainAck kW gain acknowledgment flag
 */
void processBUAcknowledgment(uint32_t boardId, bool currentGainAck, bool kwGainAck)
{
    uint32_t boardIndex = boardId - FIRST_BU_ID;  // Convert CAN ID to board index

    if (boardIndex < MAX_BU_BOARDS)
    {
        boardAckStatus[boardIndex].currentGainAck |= currentGainAck;
        boardAckStatus[boardIndex].kwGainAck |= kwGainAck;

        // Check if both acknowledgments received
        if (boardAckStatus[boardIndex].currentGainAck &&
            boardAckStatus[boardIndex].kwGainAck)
        {
            boardAckStatus[boardIndex].transmissionComplete = true;
        }
    }
}

/**
 * @brief Send calibration data to board with retry mechanism
 * @param boardIndex Index of the board in array
 * @return true if transmission successful, false otherwise
 */
bool sendBoardCalibrationDataWithRetry(uint32_t boardIndex)
{
    if (!(activeBUMask & (1UL << boardIndex)) || boardIndex >= MAX_BU_BOARDS)
    {
        return false;
    }

    // Initialize acknowledgment status for this board
    boardAckStatus[boardIndex].currentGainAck = false;
    boardAckStatus[boardIndex].kwGainAck = false;
    boardAckStatus[boardIndex].retryCount = 0;
    boardAckStatus[boardIndex].transmissionComplete = false;

    // Continue until max retries reached or successful transmission
    while (boardAckStatus[boardIndex].retryCount < 3 &&
           !boardAckStatus[boardIndex].transmissionComplete)
    {
        // Send data
        sendBoardCalibrationData(boardIndex);
        boardAckStatus[boardIndex].retryCount++;

        // Start 2-second timer
        startRetryTimer();

        // Wait for acknowledgment or timer expiry
        while (!retryTimerExpired &&
               !boardAckStatus[boardIndex].transmissionComplete)
        {
            // Continue checking for acknowledgment
            // (Acknowledgments are processed in CAN ISR via processBUAcknowledgment)
        }

        // Stop timer
        stopRetryTimer();

        // If transmission complete, return success
        if (boardAckStatus[boardIndex].transmissionComplete)
        {
            return true;
        }

        // Add small delay before retry
        DEVICE_DELAY_US(10000);  // 10ms delay
    }

    return boardAckStatus[boardIndex].transmissionComplete;
}

/**
 * @brief Send calibration data to all active boards with retry
 * @return true if all boards acknowledged, false if any board failed
 */
bool sendAllBoardsCalibrationDataWithRetry(void)
{
    uint32_t boardIndex;
    bool allSuccess = true;

    for (boardIndex = 0; boardIndex < MAX_BU_BOARDS; boardIndex++)
    {
        if (activeBUMask & (1UL << boardIndex))
        {
            bool success = sendBoardCalibrationDataWithRetry(boardIndex);
            if (!success)
            {
                allSuccess = false;
            }

            // Small delay between boards
            DEVICE_DELAY_US(1000);
        }
    }

    return allSuccess;
}

/**
 * @brief Handle BU board acknowledgment
 * @param rxMsg Pointer to received CAN message
 */
void handleBUAcknowledgment(const MCAN_RxBufElement *rxMsg)
{
    uint32_t boardId = (rxMsg->id >> 18U) & 0x1F;
    uint32_t boardIndex = boardId - FIRST_BU_ID;

    if (boardIndex < MAX_BU_BOARDS)
    {
        // Check if this is an acknowledgment message
        if (rxMsg->data[0] == (101 + boardIndex))
        {
            bool currentGainAck = (rxMsg->data[1] == 1);
            bool kwGainAck = (rxMsg->data[2] == 1);

            processBUAcknowledgment(boardId, currentGainAck, kwGainAck);
        }
    }
}

void startSync()
{
    // Configure GPIO 28 as output
     GPIO_setPadConfig(28, GPIO_PIN_TYPE_STD);    // Standard push-pull output
     GPIO_setDirectionMode(28, GPIO_DIR_MODE_OUT); // Configure as output
     GPIO_setQualificationMode(28, GPIO_QUAL_SYNC); // Synchronous qualification

     // Set GPIO 28 high
     GPIO_writePin(28, 1);  // Set the pin high (1)
}
