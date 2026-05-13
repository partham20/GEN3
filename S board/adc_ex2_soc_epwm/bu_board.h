/**
 * @file bu_board.h
 * @brief BU board discovery and communication management
 */

#ifndef BU_BOARD_H
#define BU_BOARD_H

#include "common.h"

// BU Board data structure
typedef struct {
    uint16_t currentGains[CTS_PER_BOARD];
    uint16_t kwGains[CTS_PER_BOARD];
    bool currentFrameReceived;
    bool kwFrameReceived;
    bool isActive;            // Flag to indicate if board is active
} BUBoardData;

// Structure to track board acknowledgment status
typedef struct {
    bool currentGainAck;
    bool kwGainAck;
    uint32_t retryCount;
    bool transmissionComplete;
} BoardAckStatus;

// Structure for discovered boards during discovery phase
typedef struct {
    uint32_t boardId;
    bool acknowledged;
} DiscoveredBoard;

// Board discovery functions
void initDiscoveryTracking(void);
void startBUDiscovery(void);
void finalizeBUDiscovery(void);
void startNewDiscovery(void);
void sendDiscoveryStartMessage(void);
void sendDiscoveryStopMessage(void);
void processBUDiscoveryResponse(uint32_t boardID);
bool isBoardAcknowledged(uint32_t boardID);
void recordBoardAcknowledgment(uint32_t boardID);
void sendDiscoveryResponseAck(uint32_t boardId);
void sendDuplicateIDError(uint32_t boardID);

// BU board data collection functions
void initBUTracking(void);
void startNewBUDataCollection(void);
void updateReceivedBuffer(uint32_t boardIndex, uint8_t frameType);
void sendBUFrameAck(uint32_t boardId, bool currentGainReceived, bool kwGainReceived);
void processBUMessages(void);
void processBUFrameInISR(const MCAN_RxBufElement *rxMsg);
bool checkAllFramesReceived(void);
void getMissingFrames(uint32_t *missingCurrent, uint32_t *missingKW);
uint8_t getNumActiveBUBoards(void);
bool isBoardActive(uint32_t boardID);
void syncBUBoardsData(void);
void copyBUBoardsToWorkingStructure(PDUData *pduData);

//Sync Signal
void startSync(void);

// BU board calibration data functions
void sendStopCalibrationToBUBoards(void);
void relayCommandToBUBoards(const MCAN_RxBufElement *rxMsg);
void copyFlashToBUBoards(void);
void sendBoardCalibrationData(uint32_t boardIndex);
void sendAllBoardsCalibrationData(void);
bool sendBoardCalibrationDataWithRetry(uint32_t boardIndex);
bool sendAllBoardsCalibrationDataWithRetry(void);
void processBUAcknowledgment(uint32_t boardId, bool currentGainAck, bool kwGainAck);
void handleBUAcknowledgment(const MCAN_RxBufElement *rxMsg);

// External variable declarations
extern BUBoardData buBoards[MAX_BU_BOARDS];
extern BoardAckStatus boardAckStatus[MAX_BU_BOARDS];
extern DiscoveredBoard discoveredBoardsInfo[MAX_BU_BOARDS];
extern uint32_t activeBUMask;
extern uint32_t discoveryMask;
extern uint8_t numActiveBUBoards;
extern bool discoveryInProgress;
extern uint32_t receivedCurrentFrames;
extern uint32_t receivedKWFrames;
extern bool duplicateIDFound;
extern uint32_t duplicateIDMask;
extern uint32_t acknowledgedBoardCount;
extern bool buMessagePending[MCAN_RX_BUFF_NUM];
extern bool buMessageReceived;
extern bool buDataCollectionActive;
extern bool buDataCollectionComplete;
extern bool buDataProcessed;

#endif // BU_BOARD_H
