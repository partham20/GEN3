/*
 * can_isr.c
 *
 * MCAN Interrupt Handler - modeled on reference BU_S_BOARD11/can_module.c
 *
 * Handles:
 *   - RX FIFO 0 (CAN IDs 5, 6, 7 - voltage data)
 *   - Dedicated RX Buffer (if configured)
 *   - Error conditions (FIFO overflow, protocol errors, bus off)
 *
 *  Created on: Dec 7, 2025
 *      Author: Parthasarathy.M
 */

#include "can_bu.h"

#ifdef CAN_ENABLED

#include "can_isr.h"
#include "voltage_rx.h"
#include "calibration.h"
#include "pdu_manager.h"
#include "inc/hw_mcan.h"
#include "Firmware Upgrade/fw_update_bu.h"

/* Debug counters - watch these in debugger */
volatile uint32_t g_isrCallCount = 0;
volatile uint32_t g_intrStatus = 0;
volatile uint32_t g_fifoHitCount = 0;
volatile uint32_t g_rxBufHitCount = 0;
volatile uint32_t g_newDataLow = 0;
volatile uint32_t g_newDataHigh = 0;
volatile int32_t g_lastError = 0;

/* Debug: last command that was detected on RX Buffer 0 */
volatile uint16_t g_dbgCmdByte0 = 0;
volatile uint16_t g_dbgCmdBufIdx = 0xFF;

/* Calibration RX data - parsed from S-Board frames, consumed by main loop */
CalibRxData g_calibRx = { {0}, {0}, false, false, false };


/* Live bus status - updated every ISR call */
volatile uint32_t g_livePSR = 0;
volatile uint32_t g_liveECR = 0;
volatile uint32_t g_liveRXF0S = 0;

/**
 * @brief   Primary MCAN Interrupt Handler (Line 1)
 *          Modeled on reference project's MCANIntr1ISR
 *
 *          Handles RX FIFO 0 new message and Dedicated RX Buffer interrupts
 */
__interrupt void MCANIntr1ISR(void)
{
    uint32_t intrStatus;
    MCAN_RxNewDataStatus newData;
    uint32_t bufIdx;

    g_isrCallCount++;

    //
    // Get interrupt status
    //
    intrStatus = MCAN_getIntrStatus(CAN_BU_BASE);
    g_intrStatus = intrStatus;

    //
    // Clear all interrupt status flags
    //
    MCAN_clearIntrStatus(CAN_BU_BASE, intrStatus);

    //
    // Clear the interrupt line (0x2 = line 1)
    //
    MCAN_clearInterrupt(CAN_BU_BASE, 0x2U);

    //
    // Update live bus status for debugger visibility
    //
    g_livePSR   = HWREG(CAN_BU_BASE + MCAN_PSR);
    g_liveECR   = HWREG(CAN_BU_BASE + MCAN_ECR);
    g_liveRXF0S = HWREG(CAN_BU_BASE + MCAN_RXF0S);

    //
    // ==================== Handle RX FIFO 0 Events ====================
    // IDs 5, 6, 7 are all routed to FIFO 0
    // Handle: RF0N (New Message), RF0F (FIFO Full), RF0L (Message Lost)
    // Must drain FIFO on any of these events to prevent FIFO from getting stuck
    //
    if (intrStatus & (MCAN_IR_RF0N_MASK | MCAN_IR_RF0F_MASK | MCAN_IR_RF0L_MASK))
    {
        g_fifoHitCount++;

        //
        // Call voltage RX interrupt handler to drain FIFO and store frames
        //
        VoltageRx_interruptHandler(CAN_BU_BASE);
    }

    //
    // ==================== Handle Dedicated RX Buffer ====================
    //
    if ((MCAN_IR_DRX_MASK & intrStatus) == MCAN_IR_DRX_MASK)
    {
        g_rxBufHitCount++;

        //
        // Get new data status to check which buffer(s) have new data
        //
        MCAN_getNewDataStatus(CAN_BU_BASE, &newData);
        g_newDataLow = newData.statusLow;
        g_newDataHigh = newData.statusHigh;

        //
        // Check all possible buffer elements
        //
        for (bufIdx = 0; bufIdx < myMCAN0_MCAN_RX_BUFF_NUM && bufIdx < 32U; bufIdx++)
        {
            if ((newData.statusLow & (1UL << bufIdx)) != 0U)
            {
                //
                // Read message from dedicated buffer element
                //
                MCAN_readMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, bufIdx, 0, &g_rxMsg[bufIdx]);
                g_rxMsgCount++;
                g_rxMsgNewData = true;

                //
                // Extract CAN ID from message header (standard 11-bit ID)
                //
                uint16_t rxId = (uint16_t)((g_rxMsg[bufIdx].id >> 18U) & 0x7FFU);
                uint16_t byte0 = g_rxMsg[bufIdx].data[0];
                uint16_t byte1 = g_rxMsg[bufIdx].data[1];
                uint16_t byte2 = g_rxMsg[bufIdx].data[2];

                //
                // ID 4: Broadcast commands from S-Board
                //
                if (rxId == 4U)
                {
                    if (byte0 == 0x06U)
                    {
                        // Calibration stop - reset CPU immediately
                        SysCtl_resetDevice();
                    }
                    else if (byte0 == 0x0BU)
                    {
                        // Discovery start - send first response immediately, 2 more via timer
                        g_calibrationPhase = true;
                        g_discoveryAckRxd = false;
                        CAN_sendDiscoveryResponse();
                        g_discoveryRetryCount = 2U;
                    }
                    else if (byte0 == 0x0CU)
                    {
                        // Discovery stop - send calibration gains with retries
                        g_calibTxRetryCount = 4U;
                        g_calTxAckRxd = false;
                    }
                    else if (byte0 == 0x09U)
                    {
                        // Erase Bank 4 - set flag for main loop (flash ops are blocking)
                        g_cmdEraseBank4 = true;
                    }
                    else if (byte0 == 0x0AU)
                    {
                        // Write default calibration values - set flag for main loop
                        g_cmdWriteDefaults = CMD_WRITE_DEFAULTS_KEY;
                    }
                    else if (byte0 == 0x04U)
                    {
                        // Save current calibration to flash - set flag for main loop
                        g_cmdSaveCalibration = true;
                    }

                    // Check for ACK from S-Board on ID 4
                    // Format: [100+address, 1, 1]
                    if (byte0 == (100U + address) &&
                        byte1 == 0x01U && byte2 == 0x01U)
                    {
                        if (g_discoveryRetryCount > 0U)
                        {
                            g_discoveryAckRxd = true;
                            g_discoveryRetryCount = 0U;
                        }
                        if (g_calibTxRetryCount > 0U)
                        {
                            g_calTxAckRxd = true;
                            g_calibTxRetryCount = 0U;
                        }
                    }
                }

                //
                // ID (10+address): Calibration data from S-Board
                // Parse into g_calibRx struct, send ACK after each frame.
                // Main loop handles flash save when both frames received.
                //
                if (rxId == (10U + address))
                {
                    //
                    // Byte 1: Frame type (0x01 = current gains, 0x02 = kW gains)
                    // Bytes 2-37: 18 gain values, 2 bytes each, big-endian
                    //
                    if (byte1 == 0x01U)
                    {
                        // Parse current gains into temp struct
                        uint16_t ct;
                        for (ct = 0; ct < CAL_NUM_CTS; ct++)
                        {
                            uint16_t bytePos = 2U + (ct * 2U);
                            uint16_t highByte = g_rxMsg[bufIdx].data[bytePos];
                            uint16_t lowByte  = g_rxMsg[bufIdx].data[bytePos + 1U];
                            g_calibRx.currentGain[ct] = (highByte << 8U) | lowByte;
                        }
                        g_calibRx.currentRxd = true;

                        // ACK: current gains received
                        CAN_sendCalibFrameAck(true, g_calibRx.kwRxd);
                    }
                    else if (byte1 == 0x02U)
                    {
                        // Parse kW gains into temp struct
                        uint16_t ct;
                        for (ct = 0; ct < CAL_NUM_CTS; ct++)
                        {
                            uint16_t bytePos = 2U + (ct * 2U);
                            uint16_t highByte = g_rxMsg[bufIdx].data[bytePos];
                            uint16_t lowByte  = g_rxMsg[bufIdx].data[bytePos + 1U];
                            g_calibRx.kwGain[ct] = (highByte << 8U) | lowByte;
                        }
                        g_calibRx.kwRxd = true;

                        // ACK: kW gains received
                        CAN_sendCalibFrameAck(g_calibRx.currentRxd, true);
                    }

                    // Signal main loop when both frames received
                    if (g_calibRx.currentRxd && g_calibRx.kwRxd)
                    {
                        g_calibRx.dataReady = true;
                    }
                }

                //
                // ID 0x30 / 0x31: BU firmware OTA from S-Board
                // (fw_bu_master on the S-Board is streaming into our
                // Bank 2 staging area; BU_Fw_isrOnFrame enqueues into
                // a ring buffer that the main loop drains to flash.)
                //
                if (rxId == BU_FW_DATA_CAN_ID || rxId == BU_FW_CMD_CAN_ID)
                {
                    BU_Fw_isrOnFrame(rxId, &g_rxMsg[bufIdx]);
                }
            }
        }

        //
        // Clear the NewData status - MUST be done to receive next message
        //
        MCAN_clearNewDataStatus(CAN_BU_BASE, &newData);
    }

    //
    // ==================== Handle Errors ====================
    //
    if ((intrStatus & (MCAN_IR_PED_MASK | MCAN_IR_PEA_MASK | MCAN_IR_BO_MASK)) != 0U)
    {
        g_lastError = intrStatus;
    }

    //
    // Acknowledge this interrupt to PIE Group 9
    //
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

/**
 * @brief   SysConfig-referenced ISR for MCAN Line 0 - redirects to MCANIntr1ISR
 */
__interrupt void INT_myMCAN0_0_ISR(void)
{
    MCANIntr1ISR();
}

/**
 * @brief   SysConfig-referenced ISR for MCAN Line 1 - redirects to MCANIntr1ISR
 */
__interrupt void INT_myMCAN0_1_ISR(void)
{
    MCANIntr1ISR();
}

#else /* !CAN_ENABLED */

/*
 * Stub ISR implementations when CAN is disabled
 * These are needed because SysConfig board.c references them
 */
#include <stdint.h>
#include "driverlib.h"
#include "device.h"

__interrupt void INT_myMCAN0_0_ISR(void)
{
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

__interrupt void INT_myMCAN0_1_ISR(void)
{
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

#endif /* CAN_ENABLED */
