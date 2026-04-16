/*
 * can_bu.h
 *
 * MCAN (CAN-FD) Configuration, Filters, and TX Functions
 * Full custom initialization (independent of SysConfig MCAN init)
 *
 *  Created on: Dec 7, 2025
 *      Author: Parthasarathy.M
 */

#ifndef SRC_CAN_BU_H_
#define SRC_CAN_BU_H_

#define CAN_ENABLED

#ifdef CAN_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "driverlib.h"
#include "device.h"
#include "can_config.h"
#include "power_config.h"  /* Central configuration for sample count */

//
// ====================== CAN ID Definitions ======================
//
#define CAN_ID_RX_BUFFER        (4U)    /* ID for dedicated buffer messages */
#define CAN_ID_VOLTAGE_FRAMES   (5U)    /* ID for voltage frame data (FIFO) */

//
// ====================== MCAN Message RAM Configuration ======================
// Full custom configuration (reference: BU_S_BOARD11 config.h)
//
#define MCAN_STD_ID_FILTER_NUM          (8U)      /* 5,6,7 FIFO0; 4 RXBuf0; (10+addr) RXBuf1; 0x30 FW data RXBuf2; 0x31 FW cmd RXBuf3; reject-all */
#define MCAN_EXT_ID_FILTER_NUM          (0U)

#define MCAN_FIFO_0_NUM                 (21U)     /* RX FIFO 0 depth for voltage frames */
#define MCAN_FIFO_0_ELEM_SIZE           (MCAN_ELEM_SIZE_64BYTES)
#define MCAN_FIFO_1_NUM                 (0U)
#define MCAN_FIFO_1_ELEM_SIZE           (MCAN_ELEM_SIZE_64BYTES)

#define MCAN_RX_BUFF_NUM_CFG            (5U)      /* Dedicated RX buffers */
#define MCAN_RX_BUFF_ELEM_SIZE          (MCAN_ELEM_SIZE_64BYTES)

#define MCAN_TX_BUFF_SIZE               (5U)      /* TX buffers */
#define MCAN_TX_FQ_SIZE                 (0U)
#define MCAN_TX_BUFF_ELEM_SIZE          (MCAN_ELEM_SIZE_64BYTES)
#define MCAN_TX_EVENT_SIZE              (5U)

//
// Auto-calculated Message RAM Starting Addresses
//
#define MCAN_STD_ID_FILT_START_ADDR     (0x0U)
#define MCAN_EXT_ID_FILT_START_ADDR     (MCAN_STD_ID_FILT_START_ADDR + ((MCAN_STD_ID_FILTER_NUM * MCANSS_STD_ID_FILTER_SIZE_WORDS * 4U)))
#define MCAN_FIFO_0_START_ADDR          (MCAN_EXT_ID_FILT_START_ADDR + ((MCAN_EXT_ID_FILTER_NUM * MCANSS_EXT_ID_FILTER_SIZE_WORDS * 4U)))
#define MCAN_FIFO_1_START_ADDR          (MCAN_FIFO_0_START_ADDR + (MCAN_getMsgObjSize(MCAN_FIFO_0_ELEM_SIZE) * 4U * MCAN_FIFO_0_NUM))
#define MCAN_RX_BUFF_START_ADDR         (MCAN_FIFO_1_START_ADDR + (MCAN_getMsgObjSize(MCAN_FIFO_1_ELEM_SIZE) * 4U * MCAN_FIFO_1_NUM))
#define MCAN_TX_BUFF_START_ADDR         (MCAN_RX_BUFF_START_ADDR + (MCAN_getMsgObjSize(MCAN_RX_BUFF_ELEM_SIZE) * 4U * MCAN_RX_BUFF_NUM_CFG))
#define MCAN_TX_EVENT_START_ADDR        (MCAN_TX_BUFF_START_ADDR + (MCAN_getMsgObjSize(MCAN_TX_BUFF_ELEM_SIZE) * 4U * (MCAN_TX_BUFF_SIZE + MCAN_TX_FQ_SIZE)))

//
// ====================== TX Frame Definitions ======================
//
#define CAN_VOLTAGE_BUFFER_SIZE     (640U)  /* 640 voltage samples (TX specific) */
#define CAN_VALUES_PER_FRAME        (31U)   /* 31 16-bit values per CAN frame (62 bytes / 2) */
#define CAN_FRAMES_PER_PHASE        ((CAN_VOLTAGE_BUFFER_SIZE + CAN_VALUES_PER_FRAME - 1U) / CAN_VALUES_PER_FRAME)

//
// ====================== RX Buffer Definitions ======================
//
#ifndef myMCAN0_MCAN_RX_BUFF_NUM
#define myMCAN0_MCAN_RX_BUFF_NUM    (5U)
#endif

//
// ====================== Global Variables ======================
//
extern unsigned int address;

/* RX Buffer array for dedicated buffer messages */
extern MCAN_RxBufElement g_rxMsg[myMCAN0_MCAN_RX_BUFF_NUM];
extern volatile uint32_t g_rxMsgCount;
extern volatile bool g_rxMsgNewData;

/* Debug variables */
extern volatile uint32_t g_dbgFilter0;
extern volatile uint32_t g_dbgFilter1;
extern volatile uint32_t g_dbgFilter2;
extern volatile uint32_t g_dbgSIDFC;
extern volatile uint32_t g_dbgRXBC;

/* Additional CAN status debug variables */
extern volatile uint32_t g_dbgPSR;        // Protocol Status Register
extern volatile uint32_t g_dbgECR;        // Error Counter Register
extern volatile uint32_t g_dbgCCCR;       // CC Control Register
extern volatile uint32_t g_dbgRXF0S;      // RX FIFO 0 Status
extern volatile uint32_t g_dbgIR;         // Interrupt Register
extern volatile uint32_t g_dbgIE;         // Interrupt Enable Register
extern volatile uint32_t g_dbgILS;        // Interrupt Line Select
extern volatile uint32_t g_dbgILE;        // Interrupt Line Enable

//
// ====================== MCAN Init & Config Functions ======================
//
void MCANConfig(void);
void MCANIntrConfig(void);

//
// ====================== Filter & DIP Switch Functions ======================
//
void initDIPSwitchGPIO(void);
void readCANAddress(void);
uint32_t readDIPSwitch(uint32_t gpioNumber);

//
// ====================== CAN TX Functions ======================
//
void CAN_sendSingleVoltageFrame(uint8_t phase, uint8_t frameNumber, int32_t *buffer, uint16_t startIndex);
void CAN_sendVoltageBuffer(int32_t *buffer);
void CAN_sendTestMessage(void);
void CAN_pollDebugRegisters(void);
void CAN_sendDiscoveryResponse(void);
void CAN_sendCalibrationGains(void);
void CAN_sendCalibFrameAck(bool currentOK, bool kwOK);

//
// ====================== CAN TX Timeout & Recovery ======================
//
void canTxWaitComplete(void);

// Error recovery counters — add to CCS watch window to monitor
extern volatile uint32_t g_canErrorRecoveryCount;
extern volatile uint32_t g_canHardResetCount;

#ifdef __cplusplus
}
#endif

#endif /* CAN_ENABLED */

#endif /* SRC_CAN_BU_H_ */
