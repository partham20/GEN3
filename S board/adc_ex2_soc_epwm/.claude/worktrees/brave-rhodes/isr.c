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
    intrStatusA = MCAN_getIntrStatus(MCANA_DRIVER_BASE);
    intrStatusB = MCAN_getIntrStatus(MCANB_DRIVER_BASE);

    // Clear interrupts
    if (intrStatusA != 0) {
        MCAN_clearIntrStatus(MCANA_DRIVER_BASE, intrStatusA);
        MCAN_clearInterrupt(MCANA_DRIVER_BASE, 0x2);
    }

    if (intrStatusB != 0) {
        MCAN_clearIntrStatus(MCANB_DRIVER_BASE, intrStatusB);
        MCAN_clearInterrupt(MCANB_DRIVER_BASE, 0x2);
    }

    // ============================================================================
    // CRITICAL FIX: Handle TX completion interrupts for MCANA
    // ============================================================================
    // Without this check, any code waiting on MCAN_getTxBufReqPend(MCANA_DRIVER_BASE)
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
        MCAN_getRxFIFOStatus(MCANA_DRIVER_BASE, &rxFIFO0Status);

        while (rxFIFO0Status.fillLvl > 0)
        {
            uint32_t getIndex = rxFIFO0Status.getIdx;

            MCAN_readMsgRam(MCANA_DRIVER_BASE, MCAN_MEM_TYPE_FIFO, 0U,
                            MCAN_RX_FIFO_NUM_0, &rxFIFO0Msg[getIndex]);

            MCAN_writeRxFIFOAck(MCANA_DRIVER_BASE, MCAN_RX_FIFO_NUM_0, getIndex);
            MCAN_getRxFIFOStatus(MCANA_DRIVER_BASE, &rxFIFO0Status);
        }
    }

    // Process FIFO0 messages from MCANB (M-Board calibration data — dedicated buffer)
    if ((intrStatusB & MCAN_IR_RF0N_MASK) == MCAN_IR_RF0N_MASK)
    {
        fifo = true;
        rxFIFO0Status.num = MCAN_RX_FIFO_NUM_0;
        MCAN_getRxFIFOStatus(MCANB_DRIVER_BASE, &rxFIFO0Status);

        while (rxFIFO0Status.fillLvl > 0)
        {
            uint32_t getIndex = rxFIFO0Status.getIdx;

            MCAN_readMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_FIFO, 0U,
                            MCAN_RX_FIFO_NUM_0, &rxFIFO0MsgB[getIndex]);

            uint16_t order = (rxFIFO0MsgB[getIndex].data[0] << 8) | rxFIFO0MsgB[getIndex].data[1];
            processReceivedFrame(order);

            if (getIndex == 14)
            {
                bufferFull = true;
            }

            MCAN_writeRxFIFOAck(MCANB_DRIVER_BASE, MCAN_RX_FIFO_NUM_0, getIndex);
            MCAN_getRxFIFOStatus(MCANB_DRIVER_BASE, &rxFIFO0Status);
        }
    }

    // Process dedicated buffer messages from MCANA
    if ((intrStatusA & MCAN_IR_DRX_MASK) == MCAN_IR_DRX_MASK)
    {
        MCAN_getNewDataStatus(MCANA_DRIVER_BASE, &newData);
        int i;
        for (i = 0; i < MCAN_RX_BUFF_NUM; i++)
        {
            if ((newData.statusLow & (1UL << i)) != 0)
            {
                // Copy message
                MCAN_readMsgRam(MCANA_DRIVER_BASE, MCAN_MEM_TYPE_BUF, i, 0, &rxMsg[i]);

                uint32_t boardId = (rxMsg[i].id >> 18U) & 0x1F;
                if (boardId >= FIRST_BU_ID && boardId < (FIRST_BU_ID + MAX_BU_BOARDS))
                {
                    buMessagePending[i] = true;          // Existing calibration system
                    buMessageReceived = true;            // Existing flag

                    // Copy runtime data to buffer IMMEDIATELY in ISR
                    processBURuntimeFrame(&rxMsg[i]);
                    runtimeMessageReceived = true;       // Signal main loop
                }
                else if (boardId == 3)
                {
                    CommandReceived = true;
                }

                if (rxMsg[i].data[1] == 1 && rxMsg[i].data[2] == 1) // ack
                {
                    handleBUAcknowledgment(&rxMsg[i]);
                }
            }
        }
        MCAN_clearNewDataStatus(MCANA_DRIVER_BASE, &newData);
    }

    // Process dedicated buffer messages from MCANB
    if ((intrStatusB & MCAN_IR_DRX_MASK) == MCAN_IR_DRX_MASK)
    {
        MCAN_getNewDataStatus(MCANB_DRIVER_BASE, &newData);
        int i;
        for (i = 0; i < MCAN_RX_BUFF_NUM; i++)
        {
            if ((newData.statusLow & (1UL << i)) != 0)
            {
                // Copy message from MCANB to the same rxMsg buffer
                MCAN_readMsgRam(MCANB_DRIVER_BASE, MCAN_MEM_TYPE_BUF, i, 0, &rxMsg[i]);

                uint32_t boardId = (rxMsg[i].id >> 18U) & 0x1F;

                // Check if this is a command message (ID 3)
                if (boardId == 3)
                {
                    CommandReceived = true;
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
        MCAN_clearNewDataStatus(MCANB_DRIVER_BASE, &newData);
    }

    // Acknowledge this interrupt located in group 9
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}