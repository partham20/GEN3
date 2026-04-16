/*
 * can_bu.c
 *
 * MCAN (CAN-FD) Full Custom Initialization, Filters, and TX Functions
 * Modeled on reference project: BU_S_BOARD11/can_module.c
 *
 *  Created on: Dec 7, 2025
 *      Author: Parthasarathy.M
 */

#include "can_bu.h"
#include "can_isr.h"
#include "calibration.h"
#include "pdu_manager.h"
#include "Firmware Upgrade/fw_upgrade_config.h"   /* FW_BU_MCAN_TX_PIN / _RX_PIN */

#ifdef CAN_ENABLED

#include "board.h"

//
// Global Variables
//
unsigned int address = 0;

//
// RX Buffer array for dedicated buffer messages
//
MCAN_RxBufElement g_rxMsg[myMCAN0_MCAN_RX_BUFF_NUM];
volatile uint32_t g_rxMsgCount = 0;
volatile bool g_rxMsgNewData = false;

//
// Debug variables
//
volatile uint32_t g_dbgFilter0 = 0;
volatile uint32_t g_dbgFilter1 = 0;
volatile uint32_t g_dbgFilter2 = 0;
volatile uint32_t g_dbgSIDFC = 0;
volatile uint32_t g_dbgRXBC = 0;

//
// Additional CAN status debug variables
//
volatile uint32_t g_dbgPSR = 0;        // Protocol Status Register
volatile uint32_t g_dbgECR = 0;        // Error Counter Register
volatile uint32_t g_dbgCCCR = 0;       // CC Control Register
volatile uint32_t g_dbgRXF0S = 0;      // RX FIFO 0 Status
volatile uint32_t g_dbgIR = 0;         // Interrupt Register
volatile uint32_t g_dbgIE = 0;         // Interrupt Enable Register
volatile uint32_t g_dbgILS = 0;        // Interrupt Line Select
volatile uint32_t g_dbgILE = 0;        // Interrupt Line Enable

//
// ====================== MCAN Full Custom Init ======================
//

/**
 * @brief Full MCAN initialization - FD mode, bit timing, message RAM, filters
 *        Modeled on reference project's MCANConfig()
 *        Filters: ID 5, 6, 7 -> RX FIFO 0 + reject-all
 */
void MCANConfig(void)
{
    MCAN_InitParams initParams;
    MCAN_MsgRAMConfigParams msgRAMConfigParams;
    MCAN_StdMsgIDFilterElement stdFiltelem;
    MCAN_BitTimingParams bitTimes;

    //
    // Initialize all structs to zero to prevent stray values
    //
    memset(&initParams, 0, sizeof(initParams));
    memset(&msgRAMConfigParams, 0, sizeof(msgRAMConfigParams));
    memset(&stdFiltelem, 0, sizeof(stdFiltelem));
    memset(&bitTimes, 0, sizeof(bitTimes));

    //
    // Configure MCAN initialization parameters - FD mode with BRS
    //
    initParams.fdMode = 0x1U;       // FD operation enabled
    initParams.brsEnable = 0x1U;    // Bit rate switching enabled

    //
    // Initialize Message RAM Sections Configuration Parameters
    //
    msgRAMConfigParams.flssa                = MCAN_STD_ID_FILT_START_ADDR;
    msgRAMConfigParams.lss                  = MCAN_STD_ID_FILTER_NUM;
    msgRAMConfigParams.rxBufStartAddr       = MCAN_RX_BUFF_START_ADDR;
    msgRAMConfigParams.rxBufElemSize        = MCAN_RX_BUFF_ELEM_SIZE;
    msgRAMConfigParams.txStartAddr          = MCAN_TX_BUFF_START_ADDR;
    msgRAMConfigParams.txBufNum             = MCAN_TX_BUFF_SIZE;
    msgRAMConfigParams.txFIFOSize           = MCAN_TX_FQ_SIZE;
    msgRAMConfigParams.txBufElemSize        = MCAN_TX_BUFF_ELEM_SIZE;
    msgRAMConfigParams.txEventFIFOStartAddr = MCAN_TX_EVENT_START_ADDR;
    msgRAMConfigParams.txEventFIFOSize      = MCAN_TX_EVENT_SIZE;
    msgRAMConfigParams.rxFIFO0startAddr     = MCAN_FIFO_0_START_ADDR;
    msgRAMConfigParams.rxFIFO0size          = MCAN_FIFO_0_NUM;
    msgRAMConfigParams.rxFIFO0OpMode        = 1U;   // FIFO Overwrite mode
    msgRAMConfigParams.rxFIFO0ElemSize      = MCAN_FIFO_0_ELEM_SIZE;

    //
    // Initialize bit timings
    // MCAN clock = 30MHz (SYSCLK 150MHz / DIV_5, set in main before this call)
    // Nominal baud = 30MHz / ((11+1) * (2+1+0+1)) = 30MHz / (12*4) ≈ 625 kbps
    // Data baud = 30MHz / ((2+1) * (2+1+0+1)) = 30MHz / (3*4) = 2.5 Mbps
    //
    // MCAN clock = 30MHz (SYSCLK 150MHz / DIV_5)
    // Nominal: 30MHz / ((5+1) * (1 + (6+1) + (1+1))) = 30MHz / (6*10) = 500 kbps, SP=80%
    // Data:    30MHz / ((0+1) * (1 + (10+1) + (2+1))) = 30MHz / (1*15) = 2 Mbps,   SP=80%
    bitTimes.nomRatePrescalar   = 0x5U;  // Nominal BRP: 6  (was 12)
    bitTimes.nomTimeSeg1        = 0x6U;  // Nominal TSEG1: 7 (was 3)
    bitTimes.nomTimeSeg2        = 0x1U;  // Nominal TSEG2: 2 (was 1)
    bitTimes.nomSynchJumpWidth  = 0x1U;  // Nominal SJW: 2   (was 1)
    bitTimes.dataRatePrescalar  = 0x0U;  // Data BRP: 1  (was 3)
    bitTimes.dataTimeSeg1       = 0xAU;  // Data TSEG1: 11 (was 3)
    bitTimes.dataTimeSeg2       = 0x2U;  // Data TSEG2: 3  (was 1)
    bitTimes.dataSynchJumpWidth = 0x2U;  // Data SJW: 3    (was 1)

    //
    // Configure GPIO pins for MCANA TX/RX operation
    // (Reference: BU_S_BOARD11/board_module.c)
    //
    /* Pin config comes from fw_upgrade_config.h — default GPIO 4 TX /
     * GPIO 5 RX (Launchpad + S-Board), override in the config header
     * for real BU-Board production hardware (GPIO 1 TX / GPIO 0 RX). */
    GPIO_setPinConfig(FW_BU_MCAN_RX_PIN);
    GPIO_setPinConfig(FW_BU_MCAN_TX_PIN);

    //
    // Wait for memory initialization to complete
    //
    while(FALSE == MCAN_isMemInitDone(CAN_BU_BASE))
    {
    }

    //
    // Put MCAN in SW initialization mode
    //
    MCAN_setOpMode(CAN_BU_BASE, MCAN_OPERATION_MODE_SW_INIT);
    while(MCAN_OPERATION_MODE_SW_INIT != MCAN_getOpMode(CAN_BU_BASE))
    {
    }

    //
    // Initialize MCAN module
    //
    MCAN_init(CAN_BU_BASE, &initParams);

    //
    // Configure Bit timings
    //
    MCAN_setBitTime(CAN_BU_BASE, &bitTimes);

    //
    // Configure Message RAM Sections
    //
    MCAN_msgRAMConfig(CAN_BU_BASE, &msgRAMConfigParams);

    //
    // ==================== Filter Configuration ====================
    // Filter 0: ID 5 -> RX FIFO 0 (R-phase voltage)
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_FIFO0;
    stdFiltelem.sft   = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 0x005U;
    stdFiltelem.sfid2 = 0x005U;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 0U, &stdFiltelem);

    //
    // Filter 1: ID 6 -> RX FIFO 0 (Y-phase voltage)
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_FIFO0;
    stdFiltelem.sft   = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 0x006U;
    stdFiltelem.sfid2 = 0x006U;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 1U, &stdFiltelem);

    //
    // Filter 2: ID 7 -> RX FIFO 0 (B-phase voltage)
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_FIFO0;
    stdFiltelem.sft   = MCAN_STDFILT_RANGE;
    stdFiltelem.sfid1 = 0x007U;
    stdFiltelem.sfid2 = 0x007U;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 2U, &stdFiltelem);

    //
    // Filter 3: ID 4 -> Dedicated RX Buffer 0 (broadcast commands from S-Board)
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft   = MCAN_STDFILT_CLASSIC;
    stdFiltelem.sfid1 = 0x004U;
    stdFiltelem.sfid2 = 0x000U;   // Buffer index 0
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 3U, &stdFiltelem);

    //
    // Filter 4: ID (10+address) -> Dedicated RX Buffer 1 (addressed commands/calibration)
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft   = MCAN_STDFILT_CLASSIC;
    stdFiltelem.sfid1 = 10U + address;
    stdFiltelem.sfid2 = 0x001U;   // Buffer index 1
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 4U, &stdFiltelem);

    //
    // Filter 5: BU FW data (ID 0x30) -> Dedicated RX Buffer 2
    // Streamed by fw_bu_master on the S-Board during a BU OTA.
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft   = MCAN_STDFILT_CLASSIC;
    stdFiltelem.sfid1 = 0x030U;
    stdFiltelem.sfid2 = 0x002U;   // Buffer index 2
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 5U, &stdFiltelem);

    //
    // Filter 6: BU FW command (ID 0x31) -> Dedicated RX Buffer 3
    // PREPARE/HEADER/VERIFY/ACTIVATE/ABORT from the S-Board.
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_RXBUFF;
    stdFiltelem.sft   = MCAN_STDFILT_CLASSIC;
    stdFiltelem.sfid1 = 0x031U;
    stdFiltelem.sfid2 = 0x003U;   // Buffer index 3
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 6U, &stdFiltelem);

    //
    // Filter 7: Reject-all (catch unmatched messages) — MUST stay last
    //
    stdFiltelem.sfec  = MCAN_STDFILTEC_REJECT;
    stdFiltelem.sft   = MCAN_STDFILT_CLASSIC;
    stdFiltelem.sfid1 = 0x000U;
    stdFiltelem.sfid2 = 0x000U;
    MCAN_addStdMsgIDFilter(CAN_BU_BASE, 7U, &stdFiltelem);

    //
    // Switch to Normal operation mode
    //
    MCAN_setOpMode(CAN_BU_BASE, MCAN_OPERATION_MODE_NORMAL);
    while(MCAN_OPERATION_MODE_NORMAL != MCAN_getOpMode(CAN_BU_BASE))
    {
    }

    //
    // Debug: Read back configuration registers
    //
    g_dbgSIDFC  = HWREG(CAN_BU_BASE + MCAN_SIDFC);
    g_dbgRXBC   = HWREG(CAN_BU_BASE + MCAN_RXBC);
    g_dbgPSR    = HWREG(CAN_BU_BASE + MCAN_PSR);
    g_dbgECR    = HWREG(CAN_BU_BASE + MCAN_ECR);
    g_dbgCCCR   = HWREG(CAN_BU_BASE + MCAN_CCCR);
    g_dbgRXF0S  = HWREG(CAN_BU_BASE + MCAN_RXF0S);
    g_dbgIR     = HWREG(CAN_BU_BASE + MCAN_IR);
    g_dbgIE     = HWREG(CAN_BU_BASE + MCAN_IE);
    g_dbgILS    = HWREG(CAN_BU_BASE + MCAN_ILS);
    g_dbgILE    = HWREG(CAN_BU_BASE + MCAN_ILE);

    //
    // Debug: Read back filter elements from message RAM
    //
    uint32_t filterStartAddr = (g_dbgSIDFC >> 2) & 0x3FFFU;
    g_dbgFilter0 = HWREG(CAN_BU_MSG_RAM_BASE + (filterStartAddr * 4U) + 0U);
    g_dbgFilter1 = HWREG(CAN_BU_MSG_RAM_BASE + (filterStartAddr * 4U) + 4U);
    g_dbgFilter2 = HWREG(CAN_BU_MSG_RAM_BASE + (filterStartAddr * 4U) + 8U);
}

/**
 * @brief Configure MCAN interrupts
 *        Enables ALL MCAN interrupts on Line 1 (matching reference project)
 *        Registers ISR via Interrupt_register
 */
void MCANIntrConfig(void)
{
    //
    // Enable ALL MCAN interrupts (reference: MCAN_INTR_MASK_ALL)
    // This ensures RF0N, DRX, error events, etc. all generate interrupts
    //
    MCAN_enableIntr(CAN_BU_BASE, MCAN_INTR_MASK_ALL, 1U);

    //
    // Route ALL interrupt sources to interrupt line 1
    //
    MCAN_selectIntrLine(CAN_BU_BASE, MCAN_INTR_MASK_ALL,
                        MCAN_INTR_LINE_NUM_1);

    //
    // Enable interrupt line 1
    //
    MCAN_enableIntrLine(CAN_BU_BASE, MCAN_INTR_LINE_NUM_1, 1U);

    //
    // Register ISR and enable interrupt in PIE
    //
    Interrupt_register(CAN_BU_INT_LINE1, &MCANIntr1ISR);
    Interrupt_enable(CAN_BU_INT_LINE1);
}

//
// ====================== DIP Switch Functions ======================
//

/**
 * @brief Initialize DIP switches for address configuration
 */
void initDIPSwitchGPIO(void)
{
    //
    // Configure GPIO40 - DIP Switch 1
    //
    GPIO_setPadConfig(40, GPIO_PIN_TYPE_PULLUP | GPIO_PIN_TYPE_INVERT);
    GPIO_setPinConfig(GPIO_40_GPIO40);
    GPIO_setDirectionMode(40, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(40, GPIO_QUAL_SYNC);

    //
    // Configure GPIO23 - DIP Switch 2
    //
    GPIO_setPadConfig(23, GPIO_PIN_TYPE_PULLUP | GPIO_PIN_TYPE_INVERT);
    GPIO_setPinConfig(GPIO_23_GPIO23);
    GPIO_setDirectionMode(23, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(23, GPIO_QUAL_SYNC);

    //
    // Configure GPIO41 - DIP Switch 3
    //
    GPIO_setPadConfig(41, GPIO_PIN_TYPE_PULLUP | GPIO_PIN_TYPE_INVERT);
    GPIO_setPinConfig(GPIO_41_GPIO41);
    GPIO_setDirectionMode(41, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(41, GPIO_QUAL_SYNC);

    //
    // Configure GPIO22 - DIP Switch 4
    //
    GPIO_setPadConfig(22, GPIO_PIN_TYPE_PULLUP | GPIO_PIN_TYPE_INVERT);
    GPIO_setPinConfig(GPIO_22_GPIO22);
    GPIO_setDirectionMode(22, GPIO_DIR_MODE_IN);
    GPIO_setQualificationMode(22, GPIO_QUAL_SYNC);

    //
    // Set qualification sampling period
    //
    GPIO_setQualificationPeriod(22, 6);
}

/**
 * @brief Read individual DIP switch state
 * @param gpioNumber GPIO pin number to read
 * @return Switch state (1 = ON, 0 = OFF)
 */
uint32_t readDIPSwitch(uint32_t gpioNumber)
{
    return (!GPIO_readPin(gpioNumber));
}

/**
 * @brief Read full CAN address from all DIP switches
 *        Updates global 'address' variable
 */
void readCANAddress(void)
{
    address = (readDIPSwitch(22) << 3) |  // DIP4 - MSB
              (readDIPSwitch(41) << 2) |  // DIP3
              (readDIPSwitch(23) << 1) |  // DIP2
              readDIPSwitch(40);          // DIP1 - LSB
}

//
// ====================== CAN TX Timeout & Recovery ======================
//

// Error recovery counters — watch in CCS debugger
volatile uint32_t g_canErrorRecoveryCount = 0;
volatile uint32_t g_canHardResetCount = 0;

// 500ms timeout using busy-wait (~75M cycles at 150MHz)
#define CAN_TX_TIMEOUT_LOOPS  75000000UL

// Max recovery attempts before giving up on this TX
#define CAN_MAX_RECOVERY_ATTEMPTS  3

/**
 * @brief Attempt to recover a stuck MCAN module.
 *        Cancels pending TX, re-inits the module, waits for bus-off recovery.
 * @return true if recovery succeeded, false otherwise
 */
static bool canTxTryRecover(void)
{
    // 1. Cancel pending TX on buffer 0
    MCAN_txBufCancellationReq(CAN_BU_BASE, 0U);
    DEVICE_DELAY_US(500);

    // 2. Force module into SW_INIT
    MCAN_setOpMode(CAN_BU_BASE, MCAN_OPERATION_MODE_SW_INIT);
    DEVICE_DELAY_US(1000);

    if (MCAN_getOpMode(CAN_BU_BASE) != MCAN_OPERATION_MODE_SW_INIT)
    {
        return false;
    }

    // 3. Re-configure
    MCANConfig();

    // 4. Verify NORMAL mode
    DEVICE_DELAY_US(1000);
    if (MCAN_getOpMode(CAN_BU_BASE) != MCAN_OPERATION_MODE_NORMAL)
    {
        return false;
    }

    // 5. Check bus-off is cleared
    MCAN_ProtocolStatus protStatus;
    MCAN_getProtocolStatus(CAN_BU_BASE, &protStatus);
    if (protStatus.busOffStatus)
    {
        DEVICE_DELAY_US(5000);
        MCAN_getProtocolStatus(CAN_BU_BASE, &protStatus);
        if (protStatus.busOffStatus)
        {
            return false;
        }
    }

    return true;
}

/**
 * @brief Wait for CAN TX buffer 0 to complete with timeout and error recovery.
 *        On error: tries to recover MCAN up to CAN_MAX_RECOVERY_ATTEMPTS times.
 *        If all attempts fail, returns (does NOT reset MCU — BU board is a slave).
 */
void canTxWaitComplete(void)
{
    uint16_t recoveryAttempts = 0;
    volatile uint32_t timeout;

retry:
    timeout = CAN_TX_TIMEOUT_LOOPS;
    while (MCAN_getTxBufReqPend(CAN_BU_BASE))
    {
        // Check for bus-off or error-passive
        MCAN_ProtocolStatus protStatus;
        MCAN_getProtocolStatus(CAN_BU_BASE, &protStatus);
        if (protStatus.busOffStatus || protStatus.errPassive)
        {
            if (recoveryAttempts < CAN_MAX_RECOVERY_ATTEMPTS)
            {
                recoveryAttempts++;
                g_canErrorRecoveryCount++;
                if (canTxTryRecover())
                {
                    return;  // Recovered — skip this frame
                }
                goto retry;
            }
            // All attempts failed — cancel and return
            g_canHardResetCount++;
            MCAN_txBufCancellationReq(CAN_BU_BASE, 0U);
            DEVICE_DELAY_US(500);
            return;
        }

        if (--timeout == 0)
        {
            if (recoveryAttempts < CAN_MAX_RECOVERY_ATTEMPTS)
            {
                recoveryAttempts++;
                g_canErrorRecoveryCount++;
                if (canTxTryRecover())
                {
                    return;
                }
                goto retry;
            }
            g_canHardResetCount++;
            MCAN_txBufCancellationReq(CAN_BU_BASE, 0U);
            DEVICE_DELAY_US(500);
            return;
        }
    }
}

//
// ====================== CAN TX Functions ======================
//

/**
 * @brief Send a single voltage frame
 * @param phase Phase identifier (1=R, 2=Y, 3=B)
 * @param frameNumber Frame sequence number (1-21)
 * @param buffer Pointer to the voltage buffer
 * @param startIndex Starting index in the buffer
 */
void CAN_sendSingleVoltageFrame(uint8_t phase, uint8_t frameNumber, int32_t *buffer, uint16_t startIndex)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    // Configure message properties
    TxMsg.id = ((uint32_t)(CAN_ID_VOLTAGE_FRAMES)) << 18U;
    TxMsg.brs = 0x1U;   // Enable bit rate switching
    TxMsg.dlc = 15U;    // 64 byte message
    TxMsg.rtr = 0U;     // Not a remote frame
    TxMsg.xtd = 0U;     // Standard ID
    TxMsg.fdf = 0x1U;   // FD format
    TxMsg.esi = 0U;     // Error state indicator
    TxMsg.efc = 1U;     // Event FIFO control
    TxMsg.mm = 0xAAU;   // Message marker

    // Prepare frame data
    uint8_t data[64] = {0};
    data[0] = phase;        // First byte: phase identifier
    data[1] = frameNumber;  // Second byte: frame sequence number

    // Pack voltage values (16-bit) into the remaining 62 bytes
    uint16_t dataIndex = 2;
    uint16_t valuesInThisFrame = CAN_VALUES_PER_FRAME;

    // Handle last frame which might have fewer values
    if ((startIndex + CAN_VALUES_PER_FRAME) > CAN_VOLTAGE_BUFFER_SIZE)
    {
        valuesInThisFrame = CAN_VOLTAGE_BUFFER_SIZE - startIndex;
    }

    uint16_t i;
    for (i = 0; i < valuesInThisFrame; i++)
    {
        uint16_t voltageValue = (uint16_t)(buffer[startIndex + i] & 0xFFFF);
        data[dataIndex++] = (voltageValue >> 8) & 0xFF;  // High byte
        data[dataIndex++] = voltageValue & 0xFF;         // Low byte
    }

    // Copy data to message
    memcpy(TxMsg.data, data, 64);

    // Send message
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();
}

/**
 * @brief Send all voltage buffer frames (21 frames)
 * @param buffer Pointer to the voltage buffer (640 samples)
 */
void CAN_sendVoltageBuffer(int32_t *buffer)
{
    uint8_t frameNumber;
    uint16_t startIndex;

    for (frameNumber = 1; frameNumber <= CAN_FRAMES_PER_PHASE; frameNumber++)
    {
        startIndex = (frameNumber - 1) * CAN_VALUES_PER_FRAME;
        CAN_sendSingleVoltageFrame(1, frameNumber, buffer, startIndex);
    }
}

/**
 * @brief Send a test CAN message (for initialization/testing)
 */
void CAN_sendTestMessage(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id = ((uint32_t)(CAN_ID_VOLTAGE_FRAMES)) << 18U;
    TxMsg.brs = 0x1U;
    TxMsg.dlc = 15U;
    TxMsg.rtr = 0U;
    TxMsg.xtd = 0U;
    TxMsg.fdf = 0x1U;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm = 0xAAU;

    uint8_t testData[64] = { 0x75, 0x00 };
    memcpy(TxMsg.data, testData, 64);

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();
}

/**
 * @brief Poll MCAN debug registers continuously from main loop
 */
void CAN_pollDebugRegisters(void)
{
    g_dbgPSR   = HWREG(CAN_BU_BASE + MCAN_PSR);
    g_dbgECR   = HWREG(CAN_BU_BASE + MCAN_ECR);
    g_dbgCCCR  = HWREG(CAN_BU_BASE + MCAN_CCCR);
    g_dbgRXF0S = HWREG(CAN_BU_BASE + MCAN_RXF0S);
    g_dbgIR    = HWREG(CAN_BU_BASE + MCAN_IR);
    g_dbgIE    = HWREG(CAN_BU_BASE + MCAN_IE);
    g_dbgILS   = HWREG(CAN_BU_BASE + MCAN_ILS);
    g_dbgILE   = HWREG(CAN_BU_BASE + MCAN_ILE);
}

/**
 * @brief Send discovery response to S-Board
 *        Format: [100+address, 0x01, 0x01] on ID (10+address)
 */
void CAN_sendDiscoveryResponse(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)(10U + address)) << 18U;
    TxMsg.brs = 0x1U;
    TxMsg.dlc = 15U;
    TxMsg.rtr = 0U;
    TxMsg.xtd = 0U;
    TxMsg.fdf = 0x1U;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xAAU;

    TxMsg.data[0] = 100U + address;
    TxMsg.data[1] = 0x01U;
    TxMsg.data[2] = 0x01U;

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();
}

/**
 * @brief Send calibration gains to S-Board (2 frames)
 *        Frame 1: [100+address, 0x01, 18 current gains big-endian]
 *        Frame 2: [100+address, 0x02, 18 kW gains big-endian]
 *        Each gain: uint16_t = gain * CAL_TX_GAIN_SCALE, 2 bytes big-endian
 */
void CAN_sendCalibrationGains(void)
{
    MCAN_TxBufElement TxMsg;
    uint16_t ct;
    uint16_t bytePos;

    //
    // Frame 1: Current gains (data[1] = 0x01)
    //
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
    TxMsg.id  = ((uint32_t)(10U + address)) << 18U;
    TxMsg.brs = 0x1U;
    TxMsg.dlc = 15U;
    TxMsg.rtr = 0U;
    TxMsg.xtd = 0U;
    TxMsg.fdf = 0x1U;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xAAU;

    TxMsg.data[0] = 100U + address;
    TxMsg.data[1] = 0x01U;

    for (ct = 0; ct < CAL_NUM_CTS; ct++)
    {
        uint16_t rawGain = pduManager.workingData.currentGain[ct];
        bytePos = 2U + (ct * 2U);
        TxMsg.data[bytePos]      = (rawGain >> 8) & 0xFFU;   // High byte
        TxMsg.data[bytePos + 1U] = rawGain & 0xFFU;          // Low byte
    }

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();

    //
    // ~3ms delay between frames
    //
    DEVICE_DELAY_US(3000);

    //
    // Frame 2: kW gains (data[1] = 0x02)
    //
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));
    TxMsg.id  = ((uint32_t)(10U + address)) << 18U;
    TxMsg.brs = 0x1U;
    TxMsg.dlc = 15U;
    TxMsg.rtr = 0U;
    TxMsg.xtd = 0U;
    TxMsg.fdf = 0x1U;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xAAU;

    TxMsg.data[0] = 100U + address;
    TxMsg.data[1] = 0x02U;

    for (ct = 0; ct < CAL_NUM_CTS; ct++)
    {
        uint16_t rawGain = pduManager.workingData.kwGain[ct];
        bytePos = 2U + (ct * 2U);
        TxMsg.data[bytePos]      = (rawGain >> 8) & 0xFFU;   // High byte
        TxMsg.data[bytePos + 1U] = rawGain & 0xFFU;          // Low byte
    }

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();
}

/**
 * @brief Send calibration frame ACK to S-Board after receiving each frame
 *        Format: [100+address, currentOK, kwOK] on ID (10+address)
 * @param currentOK  true if current gains frame has been received
 * @param kwOK       true if kW gains frame has been received
 */
void CAN_sendCalibFrameAck(bool currentOK, bool kwOK)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)(10U + address)) << 18U;
    TxMsg.brs = 0x1U;
    TxMsg.dlc = 15U;
    TxMsg.rtr = 0U;
    TxMsg.xtd = 0U;
    TxMsg.fdf = 0x1U;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xAAU;

    TxMsg.data[0] = 100U + address;
    TxMsg.data[1] = currentOK ? 1U : 0U;
    TxMsg.data[2] = kwOK ? 1U : 0U;

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    canTxWaitComplete();
}

#endif /* CAN_ENABLED */
