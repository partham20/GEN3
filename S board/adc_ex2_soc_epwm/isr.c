/**
 * @file isr.c
 * @brief Interrupt Service Routine implementations
 * 
 * FIXED VERSION: Added TX completion interrupt handling to prevent infinite loop
 * in sendDiscoveryStartMessage() and other CAN transmission functions
 */

#include "common.h"
#include "can_driver.h"
#include "timer_driver.h"
#include "bu_board.h"
#include "m_board.h"
#include "runtimedata.h"
#include "Firmware Upgrade/fw_image_rx.h"
#include "BU Firmware Upgrade/fw_bu_image_rx.h"
#include "BU Firmware Upgrade/fw_bu_master.h"
#include "src/fw_mode.h"


/**
 * @brief MCAN Interrupt Service Routine
 * 
 * CRITICAL FIX: Added TX completion handling (lines 38-50)
 * Without this, any code waiting on MCAN_getTxBufReqPend() will hang forever!
 */
__interrupt void MCANIntr1ISR(void)
{
    uint32_t intrStatusA, intrStatusB;
    MCAN_RxFIFOStatus rxFIFO0Status;
    MCAN_RxNewDataStatus newData;

    // Get interrupt status from both CAN modules
    intrStatusA = MCAN_getIntrStatus(CAN_BU_BASE);
    intrStatusB = MCAN_getIntrStatus(CAN_MBOARD_BASE);

    // Clear interrupts
    if (intrStatusA != 0) {
        MCAN_clearIntrStatus(CAN_BU_BASE, intrStatusA);
        MCAN_clearInterrupt(CAN_BU_BASE, 0x2);
    }

    if (intrStatusB != 0) {
        MCAN_clearIntrStatus(CAN_MBOARD_BASE, intrStatusB);
        MCAN_clearInterrupt(CAN_MBOARD_BASE, 0x2);
    }

    // ============================================================================
    // CRITICAL FIX: Handle TX completion interrupts for MCANA
    // ============================================================================
    // Without this check, any code waiting on MCAN_getTxBufReqPend(CAN_BU_BASE)
    // will hang forever because the TX complete flag is never acknowledged!
    // This affects: sendDiscoveryStartMessage(), sendDiscoveryStopMessage(), 
    //               sendDiscoveryResponseAck(), and other TX functions
    if ((intrStatusA & MCAN_IR_TC_MASK) == MCAN_IR_TC_MASK)
    {
        // TX transmission completed successfully on MCANA
        // The clearIntrStatus above already cleared the interrupt flag
        // Now MCAN_getTxBufReqPend() will return 0 and code can proceed
    }

    // ============================================================================
    // CRITICAL FIX: Handle TX completion interrupts for MCANB
    // ============================================================================
    if ((intrStatusB & MCAN_IR_TC_MASK) == MCAN_IR_TC_MASK)
    {
        // TX transmission completed successfully on MCANB
        // The clearIntrStatus above already cleared the interrupt flag
        // Now MCAN_getTxBufReqPend() will return 0 and code can proceed
    }

    // Process FIFO0 messages from MCANA (BU boards only — drain and ack, no calibration logic)
    if ((intrStatusA & MCAN_IR_RF0N_MASK) == MCAN_IR_RF0N_MASK)
    {
        rxFIFO0Status.num = MCAN_RX_FIFO_NUM_0;
        MCAN_getRxFIFOStatus(CAN_BU_BASE, &rxFIFO0Status);

        while (rxFIFO0Status.fillLvl > 0)
        {
            uint32_t getIndex = rxFIFO0Status.getIdx;

            MCAN_readMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_FIFO, 0U,
                            MCAN_RX_FIFO_NUM_0, &rxFIFO0Msg[getIndex]);

            MCAN_writeRxFIFOAck(CAN_BU_BASE, MCAN_RX_FIFO_NUM_0, getIndex);
            MCAN_getRxFIFOStatus(CAN_BU_BASE, &rxFIFO0Status);
        }
    }

    // Process FIFO0 messages from MCANB (M-Board calibration data — dedicated buffer)
    if ((intrStatusB & MCAN_IR_RF0N_MASK) == MCAN_IR_RF0N_MASK)
    {
        fifo = true;
        rxFIFO0Status.num = MCAN_RX_FIFO_NUM_0;
        MCAN_getRxFIFOStatus(CAN_MBOARD_BASE, &rxFIFO0Status);

        while (rxFIFO0Status.fillLvl > 0)
        {
            uint32_t getIndex = rxFIFO0Status.getIdx;

            MCAN_readMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_FIFO, 0U,
                            MCAN_RX_FIFO_NUM_0, &rxFIFO0MsgB[getIndex]);

            uint16_t order = (rxFIFO0MsgB[getIndex].data[0] << 8) | rxFIFO0MsgB[getIndex].data[1];
            processReceivedFrame(order);

            // bufferFull is set by processReceivedFrame() when all 15 frames
            // arrive (receivedFramesMask == 0x7FFF). Do NOT set it based on
            // FIFO getIndex — that's a slot index, not a frame count.

            MCAN_writeRxFIFOAck(CAN_MBOARD_BASE, MCAN_RX_FIFO_NUM_0, getIndex);
            MCAN_getRxFIFOStatus(CAN_MBOARD_BASE, &rxFIFO0Status);
        }
    }

    // Process dedicated buffer messages from MCANA
    if ((intrStatusA & MCAN_IR_DRX_MASK) == MCAN_IR_DRX_MASK)
    {
        MCAN_getNewDataStatus(CAN_BU_BASE, &newData);
        int i;
        for (i = 0; i < MCAN_RX_BUFF_NUM; i++)
        {
            if ((newData.statusLow & (1UL << i)) != 0)
            {
                // Copy message
                MCAN_readMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, i, 0, &rxMsg[i]);

                /* Full 11-bit standard CAN ID.
                 * Originally masked to 0x1F because every existing ID
                 * fit in 5 bits, but IDs above 0x1F (e.g. the BU OTA
                 * 0x30/0x31/0x32 set) collide with BU board IDs 11..22
                 * under the 5-bit mask. Use the full 0x7FF mask. */
                uint32_t boardId = (rxMsg[i].id >> 18U) & 0x7FFU;
                if (boardId >= FIRST_BU_ID && boardId < (FIRST_BU_ID + MAX_BU_BOARDS))
                {
                    /* BU -> S result frame (push on boot + pull reply)?
                     * These carry data[0] == RESP_FW_RESULT (0x41). Peel
                     * them off into fw_mode; everything else is runtime. */
                    if (rxMsg[i].data[0] == RESP_FW_RESULT)
                    {
                        FwMode_recordBuResult(rxMsg[i].data);
                    }
                    else
                    {
                        buMessagePending[i] = true;      // Existing calibration system
                        buMessageReceived = true;        // Existing flag

                        // Copy runtime data to buffer IMMEDIATELY in ISR
                        processBURuntimeFrame(&rxMsg[i]);
                        runtimeMessageReceived = true;   // Signal main loop
                    }
                }
                else if (boardId == 3)
                {
                    CommandReceived = true;
                }
                // Firmware update: data frame (ID 6) → ring buffer
                else if (boardId == FW_DATA_CAN_ID)
                {
                    FW_ImageRx_isrOnData(&rxMsg[i]);
                }
                // Firmware update: command frame (ID 7).
                // First give fw_mode a chance (ENTER/EXIT/STATUS_REQ use this
                // ID too but are dispatched by data[0]); fall through to the
                // OTA protocol handler otherwise.
                else if (boardId == FW_CMD_CAN_ID)
                {
                    if (!FwMode_isrOnCommand(rxMsg[i].data))
                    {
                        FW_ImageRx_isrOnCommand(&rxMsg[i]);
                    }
                }
                // BU firmware staging: data frame (ID 0x18) → Bank 1 ring buffer
                else if (boardId == FW_BU_DATA_CAN_ID)
                {
                    FW_BuImageRx_isrOnData(&rxMsg[i]);
                }
                // BU firmware staging: command frame (ID 0x19) → pending cmd
                else if (boardId == FW_BU_CMD_CAN_ID)
                {
                    FW_BuImageRx_isrOnCommand(&rxMsg[i]);
                }
                // BU firmware master: response from a BU board (ID 0x32)
                else if (boardId == FW_BU_TX_RESP_CAN_ID)
                {
                    FW_BuMaster_isrOnResponse(&rxMsg[i]);
                }

                if (rxMsg[i].data[1] == 1 && rxMsg[i].data[2] == 1) // ack
                {
                    handleBUAcknowledgment(&rxMsg[i]);
                }
            }
        }
        MCAN_clearNewDataStatus(CAN_BU_BASE, &newData);
    }

    // Process dedicated buffer messages from MCANB
    if ((intrStatusB & MCAN_IR_DRX_MASK) == MCAN_IR_DRX_MASK)
    {
        MCAN_getNewDataStatus(CAN_MBOARD_BASE, &newData);
        int i;
        for (i = 0; i < MCAN_RX_BUFF_NUM; i++)
        {
            if ((newData.statusLow & (1UL << i)) != 0)
            {
                // Copy message from MCANB to the same rxMsg buffer
                MCAN_readMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, i, 0, &rxMsg[i]);

                /* Full 11-bit standard CAN ID.
                 * Originally masked to 0x1F because every existing ID
                 * fit in 5 bits, but IDs above 0x1F (e.g. the BU OTA
                 * 0x30/0x31/0x32 set) collide with BU board IDs 11..22
                 * under the 5-bit mask. Use the full 0x7FF mask. */
                uint32_t boardId = (rxMsg[i].id >> 18U) & 0x7FFU;

                // Check if this is a command message (ID 3)
                if (boardId == 3)
                {
                    CommandReceived = true;
                }
                // Firmware update: data frame (ID 6) → ring buffer
                else if (boardId == FW_DATA_CAN_ID)
                {
                    FW_ImageRx_isrOnData(&rxMsg[i]);
                }
                // Firmware update: command frame (ID 7).
                // First give fw_mode a chance (ENTER/EXIT/STATUS_REQ use this
                // ID too but are dispatched by data[0]); fall through to the
                // OTA protocol handler otherwise.
                else if (boardId == FW_CMD_CAN_ID)
                {
                    if (!FwMode_isrOnCommand(rxMsg[i].data))
                    {
                        FW_ImageRx_isrOnCommand(&rxMsg[i]);
                    }
                }
                // BU firmware staging: data frame (ID 0x18) → Bank 1 ring buffer
                else if (boardId == FW_BU_DATA_CAN_ID)
                {
                    FW_BuImageRx_isrOnData(&rxMsg[i]);
                }
                // BU firmware staging: command frame (ID 0x19) → pending cmd
                else if (boardId == FW_BU_CMD_CAN_ID)
                {
                    FW_BuImageRx_isrOnCommand(&rxMsg[i]);
                }
                // BU firmware master: response from a BU board (ID 0x32)
                else if (boardId == FW_BU_TX_RESP_CAN_ID)
                {
                    FW_BuMaster_isrOnResponse(&rxMsg[i]);
                }
                // Process BU board messages
                else if (boardId >= FIRST_BU_ID && boardId < (FIRST_BU_ID + MAX_BU_BOARDS))
                {
                    buMessagePending[i] = true;          // Existing calibration system
                    buMessageReceived = true;            // Existing flag

                    // Copy runtime data to buffer IMMEDIATELY in ISR
                    processBURuntimeFrame(&rxMsg[i]);
                    runtimeMessageReceived = true;       // Signal main loop
                }

                // Handle acknowledgments
                if (rxMsg[i].data[1] == 1 && rxMsg[i].data[2] == 1) // ack
                {
                    handleBUAcknowledgment(&rxMsg[i]);
                }
            }
        }
        MCAN_clearNewDataStatus(CAN_MBOARD_BASE, &newData);
    }

    // Acknowledge this interrupt located in group 9
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}