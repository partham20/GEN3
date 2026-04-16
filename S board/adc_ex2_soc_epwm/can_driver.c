/**
 * @file can_driver.c
 * @brief CAN interface configuration and management implementation
 */

#include "can_driver.h"
#include "pdu_manager.h"
#include "bu_board.h"
#include "timer_driver.h"
#include "flash_driver.h"
#include "BU Firmware Upgrade/fw_bu_image_rx.h"
#include "BU Firmware Upgrade/fw_bu_master.h"

// Global variables
MCAN_RxBufElement rxMsg[MCAN_RX_BUFF_NUM], rxFIFO0Msg[MCAN_FIFO_0_NUM], rxFIFO0MsgB[MCAN_FIFO_0_NUM];
MCAN_TxBufElement txMsg[NUM_OF_MSG];
bool messageReceived = false;
bool requestRetransmission = false;
uint16_t receivedFramesMask = 0;
bool allFramesReceived = false;
bool sequenceStarted = false;
uint8_t frameCounter = 0;
uint8_t retryCounter = 0;
bool bufferFull = false;
uint8_t retransmissionData[64];
bool fifo = false;
bool sendToBUBoard = false;
bool sendToMBoard = false;

uint16_t missingFrames;

// Global: CAN TX watchdog countdown — watch in CCS debugger
// Shows CPU Timer 2 raw count (counts down from ~450M to 0 over 3 seconds)
volatile uint32_t g_canTxTimeoutCounter = 0;

// Global: CAN error recovery counters — watch in CCS debugger
volatile uint32_t g_canErrorRecoveryCount = 0;   // Times we recovered without reset
volatile uint32_t g_canHardResetCount = 0;        // Times we had to hard reset

// Exact 3-second timeout using CPU Timer 2 hardware
// PRD = (3 * SYSCLK) - 1 so timer fires after exactly 3 seconds
#define CAN_TX_TIMEOUT_PERIOD  (3UL * DEVICE_SYSCLK_FREQ - 1UL)

// Max recovery attempts before hard reset
#define CAN_MAX_RECOVERY_ATTEMPTS  3

/**
 * @brief Start CPU Timer 2 as a 3-second CAN TX watchdog
 */
static void canTxStartWatchdog(void)
{
    CPUTimer_stopTimer(CPUTIMER2_BASE);
    CPUTimer_setPeriod(CPUTIMER2_BASE, CAN_TX_TIMEOUT_PERIOD);
    CPUTimer_setPreScaler(CPUTIMER2_BASE, 0);
    CPUTimer_reloadTimerCounter(CPUTIMER2_BASE);
    CPUTimer_clearOverflowFlag(CPUTIMER2_BASE);
    CPUTimer_startTimer(CPUTIMER2_BASE);
    g_canTxTimeoutCounter = CAN_TX_TIMEOUT_PERIOD;
}

/**
 * @brief Check if CAN TX watchdog has expired (3 seconds elapsed)
 * @return true if timeout, false otherwise
 */
static bool canTxIsWatchdogExpired(void)
{
    g_canTxTimeoutCounter = CPUTimer_getTimerCount(CPUTIMER2_BASE);
    return CPUTimer_getTimerOverflowStatus(CPUTIMER2_BASE);
}

/**
 * @brief Stop the CAN TX watchdog timer
 */
static void canTxStopWatchdog(void)
{
    CPUTimer_stopTimer(CPUTIMER2_BASE);
    g_canTxTimeoutCounter = 0;
}

/**
 * @brief Attempt to recover a stuck MCAN module without resetting the MCU.
 *        Cancels pending TX, re-inits the module, waits for bus-off recovery.
 * @param base MCAN module base address
 * @return true if recovery succeeded (module back in NORMAL mode), false otherwise
 */
static bool canTxTryRecover(uint32_t base)
{
    // 1. Cancel all pending TX requests (buffer 0 through 31)
    {
        uint32_t buf;
        for (buf = 0; buf < MCAN_TX_BUFF_SIZE; buf++)
        {
            MCAN_txBufCancellationReq(base, buf);
        }
    }
    DEVICE_DELAY_US(500);

    // 2. Force module into SW_INIT to clear error state
    MCAN_setOpMode(base, MCAN_OPERATION_MODE_SW_INIT);
    DEVICE_DELAY_US(1000);

    // 3. Check we entered SW_INIT
    if (MCAN_getOpMode(base) != MCAN_OPERATION_MODE_SW_INIT)
    {
        return false;
    }

    // 4. Re-configure the module
    if (base == CAN_BU_BASE)
    {
        MCANAConfig();
    }
    else
    {
        MCANBConfig();
    }

    // 5. Verify module is back in NORMAL mode
    DEVICE_DELAY_US(1000);
    if (MCAN_getOpMode(base) != MCAN_OPERATION_MODE_NORMAL)
    {
        return false;
    }

    // 6. Check bus-off is cleared
    MCAN_ProtocolStatus protStatus;
    MCAN_getProtocolStatus(base, &protStatus);
    if (protStatus.busOffStatus)
    {
        // Bus-off needs 128 × 11 recessive bits to recover (~1.4ms at 500kbps)
        DEVICE_DELAY_US(5000);
        MCAN_getProtocolStatus(base, &protStatus);
        if (protStatus.busOffStatus)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Wait for CAN TX to complete with error recovery.
 *        On timeout: tries to recover the MCAN module up to CAN_MAX_RECOVERY_ATTEMPTS
 *        times before falling back to a hard MCU reset.
 * @param base MCAN module base address (CAN_BU_BASE or CAN_MBOARD_BASE)
 */
void canTxWaitComplete(uint32_t base)
{
    uint16_t recoveryAttempts = 0;

retry:
    canTxStartWatchdog();
    while (MCAN_getTxBufReqPend(base))
    {
        // Check for bus-off or error-passive mid-wait
        MCAN_ProtocolStatus protStatus;
        MCAN_getProtocolStatus(base, &protStatus);
        if (protStatus.busOffStatus || protStatus.errPassive)
        {
            canTxStopWatchdog();
            if (recoveryAttempts < CAN_MAX_RECOVERY_ATTEMPTS)
            {
                recoveryAttempts++;
                g_canErrorRecoveryCount++;
                if (canTxTryRecover(base))
                {
                    return;  // Recovered — caller should skip this frame
                }
            }
            // Recovery failed — hard reset
            g_canHardResetCount++;
            canTxTimeoutReset();
            // never returns
        }

        if (canTxIsWatchdogExpired())
        {
            canTxStopWatchdog();
            if (recoveryAttempts < CAN_MAX_RECOVERY_ATTEMPTS)
            {
                recoveryAttempts++;
                g_canErrorRecoveryCount++;
                if (canTxTryRecover(base))
                {
                    return;  // Recovered — caller should skip this frame
                }
                goto retry;  // Retry with fresh watchdog
            }
            // All recovery attempts exhausted — hard reset
            g_canHardResetCount++;
            canTxTimeoutReset();
            // never returns
        }
    }
    canTxStopWatchdog();
}

/**
 * @brief Wait for specific CAN TX buffer(s) to complete with error recovery.
 * @param base MCAN module base address
 * @param bufMask Bitmask of buffer(s) to wait for (e.g., 1U << bufferIndex)
 */
void canTxWaitBufComplete(uint32_t base, uint32_t bufMask)
{
    uint16_t recoveryAttempts = 0;

retry:
    canTxStartWatchdog();
    while (MCAN_getTxBufReqPend(base) & bufMask)
    {
        // Check for bus-off or error-passive mid-wait
        MCAN_ProtocolStatus protStatus;
        MCAN_getProtocolStatus(base, &protStatus);
        if (protStatus.busOffStatus || protStatus.errPassive)
        {
            canTxStopWatchdog();
            if (recoveryAttempts < CAN_MAX_RECOVERY_ATTEMPTS)
            {
                recoveryAttempts++;
                g_canErrorRecoveryCount++;
                if (canTxTryRecover(base))
                {
                    return;
                }
            }
            g_canHardResetCount++;
            canTxTimeoutReset();
        }

        if (canTxIsWatchdogExpired())
        {
            canTxStopWatchdog();
            if (recoveryAttempts < CAN_MAX_RECOVERY_ATTEMPTS)
            {
                recoveryAttempts++;
                g_canErrorRecoveryCount++;
                if (canTxTryRecover(base))
                {
                    return;
                }
                goto retry;
            }
            g_canHardResetCount++;
            canTxTimeoutReset();
        }
    }
    canTxStopWatchdog();
}

/**
 * @brief CAN TX timeout handler.
 *        1. Resets MCANA to clear any stuck state
 *        2. Best-effort sends reset command to BU boards
 *        3. Resets the S-Board MCU
 */
void canTxTimeoutReset(void)
{
    DINT;  // Disable all interrupts

    // 1. Force MCANA out of any stuck state
    MCAN_setOpMode(CAN_BU_BASE, MCAN_OPERATION_MODE_SW_INIT);
    DEVICE_DELAY_US(1000);  // 1ms for mode change
    MCANAConfig();          // Reconfigure and restart

    // 2. Best-effort: send reset command to BU boards (CMD_STOP_CALIBRATION)
    //    BU boards will reset themselves upon receiving this command.
    MCAN_TxBufElement resetMsg;
    memset(&resetMsg, 0, sizeof(MCAN_TxBufElement));
    resetMsg.id = ((uint32_t)(0x4)) << 18U;  // CAN ID 4 (broadcast)
    resetMsg.brs = 0x1;
    resetMsg.dlc = 15;
    resetMsg.fdf = 0x1;
    resetMsg.efc = 1U;
    resetMsg.mm = 0xAAU;
    resetMsg.data[0] = CMD_STOP_CALIBRATION;  // 0x06
    resetMsg.data[1] = STATUS_OK;             // 0x01

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &resetMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);

    // Wait briefly for TX (best effort, ~10ms timeout)
    volatile uint32_t d;
    for (d = 0; d < 1500000; d++)
    {
        if (!MCAN_getTxBufReqPend(CAN_BU_BASE)) break;
    }

    // 3. Reset the S-Board MCU
    SysCtl_resetDevice();
    // Never returns
}

/**
 * @brief Configure MCANA module
 */
void MCANAConfig(void)
{
    MCAN_InitParams initParams;
    MCAN_MsgRAMConfigParams msgRAMConfigParams;
    MCAN_StdMsgIDFilterElement stdFiltelem;
    MCAN_BitTimingParams bitTimes;

    // Initialize all structs to zero to prevent stray values
    memset(&initParams, 0, sizeof(initParams));
    memset(&msgRAMConfigParams, 0, sizeof(msgRAMConfigParams));
    memset(&stdFiltelem, 0, sizeof(stdFiltelem));
    memset(&bitTimes, 0, sizeof(bitTimes));

    // Configure MCAN initialization parameters
    initParams.fdMode = 0x1U;    // FD operation enabled
    initParams.brsEnable = 0x1U; // Bit rate switching enabled

    // Initialize Message RAM Sections Configuration Parameters
    msgRAMConfigParams.flssa = MCAN_STD_ID_FILT_START_ADDR;
    msgRAMConfigParams.lss = MCAN_STD_ID_FILTER_NUM;
    msgRAMConfigParams.rxBufStartAddr = MCAN_RX_BUFF_START_ADDR;
    msgRAMConfigParams.rxBufElemSize = MCAN_RX_BUFF_ELEM_SIZE;
    msgRAMConfigParams.txStartAddr = MCAN_TX_BUFF_START_ADDR;
    msgRAMConfigParams.txBufNum = MCAN_TX_BUFF_SIZE;
    msgRAMConfigParams.txFIFOSize = MCAN_TX_FQ_SIZE;
    msgRAMConfigParams.txBufElemSize = MCAN_TX_BUFF_ELEM_SIZE;
    msgRAMConfigParams.txEventFIFOStartAddr = MCAN_TX_EVENT_START_ADDR;
    msgRAMConfigParams.txEventFIFOSize = MCAN_TX_EVENT_SIZE;
    msgRAMConfigParams.rxFIFO0startAddr = MCAN_FIFO_0_START_ADDR;
    msgRAMConfigParams.rxFIFO0size = MCAN_FIFO_0_NUM;
    msgRAMConfigParams.rxFIFO0OpMode = 1U; // FIFO Overwrite
    msgRAMConfigParams.rxFIFO0ElemSize = MCAN_FIFO_0_ELEM_SIZE;

    // Initialize bit timings
    // MCAN clock = 30MHz (SYSCLK 150MHz / DIV_5)
    // Nominal: 30MHz / ((5+1) * (1 + (6+1) + (1+1))) = 30MHz / (6*10) = 500 kbps, SP=80%
    // Data:    30MHz / ((0+1) * (1 + (10+1) + (2+1))) = 30MHz / (1*15) = 2 Mbps,   SP=80%
    bitTimes.nomRatePrescalar = 0x5U;      // Nominal BRP: 6  (was 12)
    bitTimes.nomTimeSeg1 = 0x6U;           // Nominal TSEG1: 7 (was 3)
    bitTimes.nomTimeSeg2 = 0x1U;           // Nominal TSEG2: 2 (was 1)
    bitTimes.nomSynchJumpWidth = 0x1U;     // Nominal SJW: 2   (was 1)

    bitTimes.dataRatePrescalar = 0x0U;     // Data BRP: 1  (was 3)
    bitTimes.dataTimeSeg1 = 0xAU;          // Data TSEG1: 11 (was 3)
    bitTimes.dataTimeSeg2 = 0x2U;          // Data TSEG2: 3  (was 1)
    bitTimes.dataSynchJumpWidth = 0x2U;    // Data SJW: 3    (was 1)

    // Wait for memory initialization to happen
    while (FALSE == MCAN_isMemInitDone(CAN_BU_BASE))
    {
    }

    // Put MCAN in SW initialization mode
    MCAN_setOpMode(CAN_BU_BASE, MCAN_OPERATION_MODE_SW_INIT);

    // Wait till MCAN is initialized
    while (MCAN_OPERATION_MODE_SW_INIT != MCAN_getOpMode(CAN_BU_BASE))
    {
    }

    // Initialize MCAN module
    MCAN_init(CAN_BU_BASE, &initParams);

    // Configure Bit timings
    MCAN_setBitTime(CAN_BU_BASE, &bitTimes);

    // Configure Message RAM Sections
    MCAN_msgRAMConfig(CAN_BU_BASE, &msgRAMConfigParams);

    // Standard Filters for Dedicated RX Buffers
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft = MCAN_STDFILT_RANGE;

    // Filter 0: CAN ID 5 -> RX buffer 0
    stdFiltelem.sfid1 = 5;
    stdFiltelem.sfid2 = 0;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 0, &stdFiltelem);

    // Filters 1-12: BU board CAN IDs 11-22 -> RX buffers 4-15
    int i;
    for (i = 0; i < MAX_BU_BOARDS; i++)
    {
        stdFiltelem.sfid1 = FIRST_BU_ID + i;   // CAN ID: 11, 12, ..., 22
        stdFiltelem.sfid2 = 4 + i;             // Buffer:  4,  5, ..., 15
        MCAN_addStdMsgIDFilter(CAN_BU_BASE, 1 + i, &stdFiltelem);
    }

    // Filter 13: M-Board commands (ID 3) -> RX Buffer 3
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 3;
    stdFiltelem.sfid2 = 3;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 13U, &stdFiltelem);

    // Filter 14: FW command (ID 7) -> RX Buffer 1
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 7;
    stdFiltelem.sfid2 = 1;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 14U, &stdFiltelem);

    // Filter 15: FW data (ID 6) -> RX Buffer 2
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 6;
    stdFiltelem.sfid2 = 2;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 15U, &stdFiltelem);

    // Filter 16: BU FW data (ID 0x18) -> RX Buffer 16
    // Used by the GUI's "BU via Bank 1" mode to stage a BU board's .bin
    // into the S-Board's Bank 1 (see BU Firmware Upgrade/fw_bu_image_rx.c)
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = FW_BU_DATA_CAN_ID;   // 0x18
    stdFiltelem.sfid2 = 16;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 16U, &stdFiltelem);

    // Filter 17: BU FW command (ID 0x19) -> RX Buffer 17
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = FW_BU_CMD_CAN_ID;    // 0x19
    stdFiltelem.sfid2 = 17;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 17U, &stdFiltelem);

    // Filter 18: BU response from a BU board (ID 0x32) -> RX Buffer 18
    // Consumed by fw_bu_master when driving a BU firmware upgrade.
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = FW_BU_TX_RESP_CAN_ID;   // 0x32
    stdFiltelem.sfid2 = 18;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 18U, &stdFiltelem);

    MCAN_setOpMode(CAN_BU_BASE, MCAN_OPERATION_MODE_NORMAL);

    while (MCAN_OPERATION_MODE_NORMAL != MCAN_getOpMode(CAN_BU_BASE))
    {
    }
}

/**
 * @brief Configure MCANB module
 */
void MCANBConfig(void)
{
    MCAN_InitParams initParams;
    MCAN_MsgRAMConfigParams msgRAMConfigParams;
    MCAN_StdMsgIDFilterElement stdFiltelem;
    MCAN_BitTimingParams bitTimes;

    // Initialize all structs to zero to prevent stray values
    memset(&initParams, 0, sizeof(initParams));
    memset(&msgRAMConfigParams, 0, sizeof(msgRAMConfigParams));
    memset(&stdFiltelem, 0, sizeof(stdFiltelem));
    memset(&bitTimes, 0, sizeof(bitTimes));

    // Configure MCAN initialization parameters
    initParams.fdMode = 0x1U;    // FD operation enabled
    initParams.brsEnable = 0x1U; // Bit rate switching enabled

    // Initialize Message RAM Sections Configuration Parameters
    msgRAMConfigParams.flssa = MCAN_STD_ID_FILT_START_ADDR;
    msgRAMConfigParams.lss = MCAN_STD_ID_FILTER_NUM;
    msgRAMConfigParams.rxBufStartAddr = MCAN_RX_BUFF_START_ADDR;
    msgRAMConfigParams.rxBufElemSize = MCAN_RX_BUFF_ELEM_SIZE;
    msgRAMConfigParams.txStartAddr = MCAN_TX_BUFF_START_ADDR;
    msgRAMConfigParams.txBufNum = MCAN_TX_BUFF_SIZE;
    msgRAMConfigParams.txFIFOSize = MCAN_TX_FQ_SIZE;
    msgRAMConfigParams.txBufElemSize = MCAN_TX_BUFF_ELEM_SIZE;
    msgRAMConfigParams.txEventFIFOStartAddr = MCAN_TX_EVENT_START_ADDR;
    msgRAMConfigParams.txEventFIFOSize = MCAN_TX_EVENT_SIZE;
    msgRAMConfigParams.rxFIFO0startAddr = MCAN_FIFO_0_START_ADDR;
    msgRAMConfigParams.rxFIFO0size = MCAN_FIFO_0_NUM;
    msgRAMConfigParams.rxFIFO0OpMode = 1U; // FIFO Overwrite
    msgRAMConfigParams.rxFIFO0ElemSize = MCAN_FIFO_0_ELEM_SIZE;

    // Initialize bit timings
    // MCAN clock = 30MHz (SYSCLK 150MHz / DIV_5)
    // Nominal: 30MHz / ((5+1) * (1 + (6+1) + (1+1))) = 30MHz / (6*10) = 500 kbps, SP=80%
    // Data:    30MHz / ((0+1) * (1 + (10+1) + (2+1))) = 30MHz / (1*15) = 2 Mbps,   SP=80%
    bitTimes.nomRatePrescalar = 0x5U;      // Nominal BRP: 6  (was 12)
    bitTimes.nomTimeSeg1 = 0x6U;           // Nominal TSEG1: 7 (was 3)
    bitTimes.nomTimeSeg2 = 0x1U;           // Nominal TSEG2: 2 (was 1)
    bitTimes.nomSynchJumpWidth = 0x1U;     // Nominal SJW: 2   (was 1)
    bitTimes.dataRatePrescalar = 0x0U;     // Data BRP: 1  (was 3)
    bitTimes.dataTimeSeg1 = 0xAU;          // Data TSEG1: 11 (was 3)
    bitTimes.dataTimeSeg2 = 0x2U;          // Data TSEG2: 3  (was 1)
    bitTimes.dataSynchJumpWidth = 0x2U;    // Data SJW: 3    (was 1)

    // Wait for memory initialization to happen
    while (FALSE == MCAN_isMemInitDone(CAN_MBOARD_BASE))
    {
    }

    // Put MCAN in SW initialization mode
    MCAN_setOpMode(CAN_MBOARD_BASE, MCAN_OPERATION_MODE_SW_INIT);

    // Wait till MCAN is initialized
    while (MCAN_OPERATION_MODE_SW_INIT != MCAN_getOpMode(CAN_MBOARD_BASE))
    {
    }

    // Initialize MCAN module
    MCAN_init(CAN_MBOARD_BASE, &initParams);

    // Configure Bit timings
    MCAN_setBitTime(CAN_MBOARD_BASE, &bitTimes);

    // Configure Message RAM Sections
    MCAN_msgRAMConfig(CAN_MBOARD_BASE, &msgRAMConfigParams);

    // Standard Filters for Dedicated RX Buffers
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft = MCAN_STDFILT_RANGE;

    stdFiltelem.sfid2 = 3;
    stdFiltelem.sfid1 = 3;
    MCAN_addStdMsgIDFilter(CAN_MBOARD_BASE, 0, &stdFiltelem);

    // Filter 1: FW command (ID 7) → Dedicated RX Buffer 1
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 7;      // FW_CMD_CAN_ID
    stdFiltelem.sfid2 = 1;      // Store in RX Buffer 1
    MCAN_addStdMsgIDFilter(CAN_MBOARD_BASE, 1U, &stdFiltelem);

    // Filter 2: FW data (ID 6) → Dedicated RX Buffer 2
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 6;      // FW_DATA_CAN_ID
    stdFiltelem.sfid2 = 2;      // Store in RX Buffer 2
    MCAN_addStdMsgIDFilter(CAN_MBOARD_BASE, 2U, &stdFiltelem);

    // Filter 3: BU FW data (ID 0x18) → Dedicated RX Buffer 16
    // Used by the GUI's "BU via Bank 1" mode (see fw_bu_image_rx.c)
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = FW_BU_DATA_CAN_ID;   // 0x18
    stdFiltelem.sfid2 = 16;
    MCAN_addStdMsgIDFilter(CAN_MBOARD_BASE, 3U, &stdFiltelem);

    // Filter 4: BU FW command (ID 0x19) → Dedicated RX Buffer 17
    stdFiltelem.sfec = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft  = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = FW_BU_CMD_CAN_ID;    // 0x19
    stdFiltelem.sfid2 = 17;
    MCAN_addStdMsgIDFilter(CAN_MBOARD_BASE, 4U, &stdFiltelem);

    // Standard Filter for RX FIFO Buffers
    stdFiltelem.sfec = MCAN_STDFILTEC_FIFO0;
    stdFiltelem.sfid2 = 0x001U;
    stdFiltelem.sfid1 = 0x001U;
    stdFiltelem.sft = MCAN_STDFILT_RANGE;
    MCAN_addStdMsgIDFilter(CAN_MBOARD_BASE, 20U, &stdFiltelem);

    // Configure reject-all filter for FIFO 0 (to discard unmatched messages)
    stdFiltelem.sfec = MCAN_STDFILTEC_REJECT;
    stdFiltelem.sfid1 = 0x000; // Don't care
    stdFiltelem.sfid2 = 0x000; // Don't care
    stdFiltelem.sft = MCAN_STDFILT_CLASSIC;
    MCAN_addStdMsgIDFilter(CAN_MBOARD_BASE, 21U, &stdFiltelem);

    MCAN_setOpMode(CAN_MBOARD_BASE, MCAN_OPERATION_MODE_NORMAL);

    while (MCAN_OPERATION_MODE_NORMAL != MCAN_getOpMode(CAN_MBOARD_BASE))
    {
    }
}

/**
 * @brief Configure MCAN interrupt
 */
void MCANIntrConfig(void)
{
    // NOTE: Do NOT call Interrupt_initModule() / Interrupt_initVectorTable() here.
    // They are already called in startup() step 3. Calling them again would
    // wipe out previously registered ISRs (DMA, CPU Timer 0, etc.).

    Interrupt_register(CAN_BU_INT_LINE1, &MCANIntr1ISR);
    Interrupt_enable(CAN_BU_INT_LINE1);

    Interrupt_register(CAN_MBOARD_INT_LINE1, &MCANIntr1ISR);
    Interrupt_enable(CAN_MBOARD_INT_LINE1);
}

/**
 * @brief Reset CAN module
 */
/**
 /**
 * @brief Handle missing frames by requesting retransmission
 */
void handleMissingFrames(void)
{
    missingFrames = 0;
    uint16_t expectedMask = 0x7FFF; // Bits 0-14 set (15 frames)

    // Find missing frames
    missingFrames = expectedMask & ~receivedFramesMask;

    if (missingFrames == 0)
    {
        return;
    }

    // Increment retry counter
    retryCounter++;

    // Check if we've exceeded max retries
    if (retryCounter >= MAX_RETRIES)
    {
        maxRetriesExceeded();
        resetCANModule();

        // CRITICAL FIX: Reset ALL tracking variables after max retries
        resetFrameTracking();

        // Reset calibration-related flags to prevent continuous attempts
        sendToBUBoard = false;
        sendToMBoard = false;

        return;
    }

    // Prepare and send retransmission request
    if (missingFrames != 0)
    {
        uint8_t retransmitData[64] = {0x03}; // Command for retransmission
        int i;

        // Convert missing frames mask to retransmission request format
        for (i = 0; i < 15; i++)
        {
            retransmitData[i + 1] = (missingFrames & (1 << i)) ? 0x00 : 0x01;
        }

        // Send retransmission request
        MCAN_TxBufElement txMsg;
        memset(&txMsg, 0, sizeof(MCAN_TxBufElement));

        txMsg.id = ((uint32_t)(0x4)) << 18U;
        txMsg.brs = 0x1;
        txMsg.dlc = 15;
        txMsg.rtr = 0;
        txMsg.xtd = 0;
        txMsg.fdf = 0x1;
        txMsg.esi = 0U;
        txMsg.efc = 1U;
        txMsg.mm = 0xAAU;

        memcpy(txMsg.data, retransmitData, 64);

        MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &txMsg);
        MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

        // Wait for transmission to complete (with timeout)
        canTxWaitComplete(CAN_MBOARD_BASE);

        requestRetransmission = false;
    }
    else
    {
        // All frames received, reset all tracking variables
        resetFrameTracking();
    }
}

/**
 * @brief Reset CAN module with proper timer handling
 */
void resetCANModule(void)
{
    // Disable interrupts temporarily
    DINT;

    // Reset MCAN module
    MCAN_setOpMode(CAN_BU_BASE, MCAN_OPERATION_MODE_SW_INIT);
    while(MCAN_OPERATION_MODE_SW_INIT != MCAN_getOpMode(CAN_BU_BASE)) {}

    MCAN_setOpMode(CAN_MBOARD_BASE, MCAN_OPERATION_MODE_SW_INIT);
    while(MCAN_OPERATION_MODE_SW_INIT != MCAN_getOpMode(CAN_MBOARD_BASE)) {}

    // Reconfigure MCAN
    MCANAConfig();
    MCANBConfig();

    // Send reset notification message
    MCAN_TxBufElement TxMsgA;
    memset(&TxMsgA, 0, sizeof(MCAN_TxBufElement));
    TxMsgA.id = ((uint32_t)(0x4)) << 18U;
    TxMsgA.brs = 0x1;
    TxMsgA.dlc = 15;
    TxMsgA.rtr = 0;
    TxMsgA.xtd = 0;
    TxMsgA.fdf = 0x1;
    TxMsgA.esi = 0U;
    TxMsgA.efc = 1U;
    TxMsgA.mm = 0xAAU;

    // Message indicating CAN reset
    uint8_t resetMsg[64] = {0x05, 0x01}; // Command 0x05 for CAN reset notification
    memcpy(TxMsgA.data, resetMsg, 64);

    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsgA);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

    // Wait for transmission (with timeout)
    canTxWaitComplete(CAN_MBOARD_BASE);

    // Re-enable interrupts
    EINT;
}

/**
 * @brief Clear RX FIFO buffer
 */
void ClearRxFIFOBuffer(void)
{
    int i, j;
    for (i = 0; i < MCAN_FIFO_0_NUM; i++)
    {
        rxFIFO0Msg[i].id = 0U;
        rxFIFO0Msg[i].rtr = 0U;
        rxFIFO0Msg[i].xtd = 0U;
        rxFIFO0Msg[i].esi = 0U;
        rxFIFO0Msg[i].rxts = 0U;
        rxFIFO0Msg[i].dlc = 0U;
        rxFIFO0Msg[i].brs = 0U;
        rxFIFO0Msg[i].fdf = 0U;
        rxFIFO0Msg[i].fidx = 0U;
        rxFIFO0Msg[i].anmf = 0U;

        for (j = 0; j < 64; j++)  // Initialize data bytes to 0
        {
            rxFIFO0Msg[i].data[j] = 0;
        }
    }
}

/**
 * @brief Flush MCANB hardware RX FIFO and reset calibration frame tracking
 *
 * Drains all pending elements from the MCANB RX FIFO 0 using the MCAN driver
 * API (MCAN_getRxFIFOStatus + MCAN_writeRxFIFOAck), clears the software
 * rxFIFO0Msg[] array, and resets the calibration frame bitmask and counters.
 */
void flushCalibrationFIFO(void)
{
    DINT;

    // 1. Drain hardware FIFO by acknowledging all pending elements
    MCAN_RxFIFOStatus fifoStatus;
    fifoStatus.num = MCAN_RX_FIFO_NUM_0;
    MCAN_getRxFIFOStatus(CAN_MBOARD_BASE, &fifoStatus);

    while (fifoStatus.fillLvl > 0)
    {
        MCAN_writeRxFIFOAck(CAN_MBOARD_BASE, MCAN_RX_FIFO_NUM_0, fifoStatus.getIdx);
        MCAN_getRxFIFOStatus(CAN_MBOARD_BASE, &fifoStatus);
    }

    // 2. Clear software rxFIFO0MsgB[] array (MCANB calibration buffer)
    int i, j;
    for (i = 0; i < MCAN_FIFO_0_NUM; i++)
    {
        rxFIFO0MsgB[i].id = 0U;
        rxFIFO0MsgB[i].rtr = 0U;
        rxFIFO0MsgB[i].xtd = 0U;
        rxFIFO0MsgB[i].esi = 0U;
        rxFIFO0MsgB[i].rxts = 0U;
        rxFIFO0MsgB[i].dlc = 0U;
        rxFIFO0MsgB[i].brs = 0U;
        rxFIFO0MsgB[i].fdf = 0U;
        rxFIFO0MsgB[i].fidx = 0U;
        rxFIFO0MsgB[i].anmf = 0U;
        for (j = 0; j < 64; j++)
        {
            rxFIFO0MsgB[i].data[j] = 0;
        }
    }

    // 3. Reset calibration frame bitmask and tracking variables
    receivedFramesMask = 0;
    allFramesReceived = false;
    sequenceStarted = false;
    frameCounter = 0;
    bufferFull = false;
    requestRetransmission = false;
    retryCounter = 0;
    timerExpired = false;

    // 4. Stop frame tracking timers
    stopSoftwareTimer(TIMER_FRAME_TRACKING);
    stopSoftwareTimer(TIMER_RETRY);
    retryTimerExpired = false;

    EINT;
}

/**
 * @brief Clear RX dedicated buffer
 */
void ClearRxDedicatedBuffer(void)
{
    int i;
    int loopCnt;

    for (loopCnt = 0; loopCnt < MCAN_RX_BUFF_NUM; loopCnt++)
    {
        // Initialize message to receive
        rxMsg[loopCnt].id = 0U;
        rxMsg[loopCnt].rtr = 0U;
        rxMsg[loopCnt].xtd = 0U;
        rxMsg[loopCnt].esi = 0U;
        rxMsg[loopCnt].rxts = 0U;   // Rx Timestamp
        rxMsg[loopCnt].dlc = 0U;
        rxMsg[loopCnt].brs = 0U;
        rxMsg[loopCnt].fdf = 0U;
        rxMsg[loopCnt].fidx = 0U;   // Filter Index
        rxMsg[loopCnt].anmf = 0U;   // Accepted Non-matching Frame

        for (i = 0; i < 64; i++)  // Initialize receive buffer to 0
        {
            rxMsg[loopCnt].data[i] = 0;
        }
    }
}

/**
 * @brief Reassemble CAN FIFO messages into receivedBuffer.
 *
 * Each FIFO message has a 2-byte order header + 62 data bytes (31 words).
 * Words are stored big-endian (MSB first) in the frame data bytes.
 * After reassembly, receivedBuffer contains PDUData (451 words):
 *   words 0..450 = PDUData fields in struct memory order.
 *
 * The caller must then invoke processReceivedCalibrationData() to
 * write to flash and send ACKs.
 *
 * @return int Always 0
 */
int copyRxFIFO(void)
{
    int i, j;
    #define MAX_MESSAGES 15
    #define MESSAGE_DATA_SIZE 62
    #define HEADER_SIZE 2

    for (i = 0; i < MAX_MESSAGES; i++)
    {
        uint16_t order = (rxFIFO0MsgB[i].data[0] << 8) | rxFIFO0MsgB[i].data[1];

        if (order == 0 || order > MAX_MESSAGES)
        {
            break;
        }

        uint32_t startPos = (order - 1) * (MESSAGE_DATA_SIZE / 2);

        if (startPos + (MESSAGE_DATA_SIZE / 2) > TOTAL_BUFFER_SIZE)
        {
            break;
        }

        for (j = 0; j < MESSAGE_DATA_SIZE; j += 2)
        {
            receivedBuffer[startPos++] = (rxFIFO0MsgB[i].data[HEADER_SIZE + j] << 8) |
                                         rxFIFO0MsgB[i].data[HEADER_SIZE + j + 1];
        }
    }

    bufferFull = false;
    return 0;
}

        /**
         * @brief Process received frame
         * @param frameNumber Frame number to process
         */
        void processReceivedFrame(uint8_t frameNumber)
        {
            // Start/restart timer for any new frame
            startFrameSequenceTimer();

            // Only process the frame if we haven't received it yet
            if (!(receivedFramesMask & (1 << (frameNumber - 1))))
            {
                // Set bit in mask for received frame
                receivedFramesMask |= (1 << (frameNumber - 1));
                frameCounter++;

                // Check if we have all frames
                if (receivedFramesMask == 0x7FFF)
                {
                    allFramesReceived = true;
                    bufferFull = true;  // Signal main loop to process the data
                   // CPUTimer_stopTimer(CPUTIMER0_BASE);
                    retryCounter = 0;   // Reset retry counter on success
                }
            }
        }

        /**
         * @brief Handle missing frames by requesting retransmission
         */


        /**
         * @brief Notify of max retries exceeded
         */
        void maxRetriesExceeded(void)
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

            uint8_t datare[64] = {0x05, 0x00};
            int i;
            for(i = 0; i < 64; i++)
            {
                TxMsg.data[i] = datare[i];
            }

            MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
            MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);

            canTxWaitComplete(CAN_MBOARD_BASE);
        }

        /**
         * @brief Transmit frames to target ID
         */
        void transmit(void)
        {
            int i, frameIndex;
            uint32_t targetId = sendToBUBoard ? 0x5 : 0x2;  // Select ID based on target

            for (i = 1; i < 16; i++)
            {
                if (retransmissionData[i] == 0x00)
                {
                    frameIndex = i - 1;

                    canTxWaitBufComplete(CAN_BU_BASE, (1U << frameIndex));

                    MCAN_TxBufElement txMsg;
                    memset(&txMsg, 0, sizeof(MCAN_TxBufElement));

                    memcpy(txMsg.data, storedFrames[frameIndex].data, 64);

                    // Set up CAN message properties with dynamic ID
                    txMsg.id = ((uint32_t)(targetId)) << 18U;
                    txMsg.dlc = 15U;
                    txMsg.fdf = 1U;
                    txMsg.brs = 1U;

                    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, frameIndex, &txMsg);
                    MCAN_txBufAddReq(CAN_BU_BASE, frameIndex);

                    canTxWaitBufComplete(CAN_BU_BASE, (1U << frameIndex));
                }
            }

            // Reset flags after transmission
            sendToBUBoard = false;
            sendToMBoard = false;
        }

        void syncCANSignal(void)
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

                   uint8_t datare[64] = {0xAA, 0xAA};
                   int i;
                   for(i = 0; i < 64; i++)
                   {
                       TxMsg.data[i] = datare[i];
                   }

                   MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
                   MCAN_txBufAddReq(CAN_BU_BASE, 0U);

                   canTxWaitComplete(CAN_BU_BASE);
               }
        void discoveryAck_ResetCANin2secondsIfNotReceived(void)
                    {
                        MCAN_TxBufElement TxMsg;
                        memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
                        TxMsg.id = ((uint32_t)(0x5)) << 18U;
                        TxMsg.brs = 0x1;
                        TxMsg.dlc = 15;
                        TxMsg.rtr = 0;
                        TxMsg.xtd = 0;
                        TxMsg.fdf = 0x1;
                        TxMsg.esi = 0U;
                        TxMsg.efc = 1U;
                        TxMsg.mm = 0xAAU;

                        uint8_t datare[64] = {0xAA, 0xAA};
                        int i;
                        for(i = 0; i < 64; i++)
                        {
                            TxMsg.data[i] = datare[i];
                        }

                        MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
                        MCAN_txBufAddReq(CAN_BU_BASE, 0U);

                        canTxWaitComplete(CAN_BU_BASE);
                    }

