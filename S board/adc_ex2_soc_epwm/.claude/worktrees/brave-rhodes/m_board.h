/**
 * @file m_board.h
 * @brief M-board communication management
 */

#ifndef M_BOARD_H
#define M_BOARD_H

#include "common.h"

// M-board command functions
void sendDiscoveryStartAck(void);
void sendCalibrationDataFormatVersion(void);
void handleMBoardRetransmissionRequest(const uint8_t *bitMask);
void sendMissingFrames(const uint8_t *retransmissionData);
void handleRetransmissionRequest(const MCAN_RxBufElement *rxMsg);
void sendCalibrationDataToMBoard(void);
void sendStopCalibrationAck(void);

// External variable declarations
extern bool CommandReceived;
extern bool versionNumberRequested;
extern bool startupReceive;
extern bool calibStart;
extern volatile bool calibrationInProgress;
extern uint32_t mBoardCommandID;

// In an appropriate header file (e.g., m_board.h)
extern bool calibDataTransmissionInProgress;
extern bool calibDataTransmissionComplete;
extern uint8_t transmittedFrameCount;
extern uint32_t lastTransmissionTime;
extern uint8_t transmissionRetryCount;
extern bool waitingForAck;

#endif // M_BOARD_H
