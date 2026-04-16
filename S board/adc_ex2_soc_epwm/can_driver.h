/**
 * @file can_driver.h
 * @brief CAN interface configuration and management
 */

#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "common.h"

// CAN initialization functions
void MCANAConfig(void);
void MCANBConfig(void);
void MCANIntrConfig(void);
void resetCANModule(void);

// Buffer management functions
void ClearRxFIFOBuffer(void);
void ClearRxDedicatedBuffer(void);
int copyRxFIFO(void);

// Frame tracking and processing
void processReceivedFrame(uint8_t frameNumber);
void handleMissingFrames(void);
void maxRetriesExceeded(void);
void transmit(void);
void discoveryAck_ResetCANin2secondsIfNotReceived ();
void syncCANSignal ();
void flushCalibrationFIFO(void);

// CAN TX timeout protection (exact 3 seconds via CPU Timer 2)
void canTxWaitComplete(uint32_t base);
void canTxWaitBufComplete(uint32_t base, uint32_t bufMask);
void canTxTimeoutReset(void);

// Global watchdog counter — add to CCS watch window to monitor
// Counts down from ~450,000,000 to 0 over 3 seconds (raw SYSCLK ticks)
extern volatile uint32_t g_canTxTimeoutCounter;

// Error recovery counters — add to CCS watch window to monitor
extern volatile uint32_t g_canErrorRecoveryCount;  // Successful recoveries (no reset)
extern volatile uint32_t g_canHardResetCount;       // Hard resets (recovery failed)

// External variables
extern MCAN_RxBufElement rxMsg[MCAN_RX_BUFF_NUM], rxFIFO0Msg[MCAN_FIFO_0_NUM], rxFIFO0MsgB[MCAN_FIFO_0_NUM];
extern MCAN_TxBufElement txMsg[NUM_OF_MSG];
extern bool messageReceived;
extern bool requestRetransmission;
extern uint16_t receivedFramesMask;
extern bool allFramesReceived;
extern bool sequenceStarted;
extern uint8_t frameCounter;
extern uint8_t retryCounter;
extern bool bufferFull;
extern uint8_t retransmissionData[64];
extern bool fifo;
extern bool sendToBUBoard;
extern bool sendToMBoard;
extern uint16_t missingFrames;

#endif // CAN_DRIVER_H
