/*
 * fw_mode.c — BU-Board FW Update Mode control
 */

#include "fw_mode.h"
#include "driverlib.h"
#include "device.h"
#include "can_bu.h"      /* CAN_BU_BASE, address */
#include <string.h>

/* Suspend/resume hooks — defined in adc_ex2_soc_epwm.c.  Stage 1
 * stubs; stage 2 will add real ePWM/DMA/ISR control. */
extern void buBoardSuspendNormalMode(void);
extern void buBoardResumeNormalMode(void);
extern unsigned int address;

static FwModeState       s_state = FW_MODE_NORMAL;
static uint32_t          s_hbCounter = 0;
static volatile uint8_t  s_pendingCmd = 0;

static uint32_t computeCRC32Image(void);
static void     sendMCANA(uint16_t canId, const uint8_t *payload, uint16_t len);
static void     sendResult(uint8_t status);
static void     enterMode(void);
static void     exitMode(void);

void FwMode_init(void)
{
    s_state = FW_MODE_NORMAL;
    s_hbCounter = 0;
    s_pendingCmd = 0;
}

FwModeState FwMode_getState(void) { return s_state; }

uint8_t FwMode_isrOnBroadcast(const uint8_t *data)
{
    uint8_t cmd = data[0];
    if (cmd == BU_CMD_ENTER_FW_MODE ||
        cmd == BU_CMD_EXIT_FW_MODE  ||
        cmd == BU_CMD_FW_STATUS_REQ)
    {
        s_pendingCmd = cmd;
        return 1U;
    }
    return 0U;
}

void FwMode_tick(void)
{
    if (s_pendingCmd != 0U)
    {
        uint8_t cmd = s_pendingCmd;
        s_pendingCmd = 0U;
        switch (cmd)
        {
            case BU_CMD_ENTER_FW_MODE:   enterMode(); break;
            case BU_CMD_EXIT_FW_MODE:    exitMode();  break;
            case BU_CMD_FW_STATUS_REQ:
                sendResult(FwMode_isActive() ? FW_RESULT_BOOTED : FW_RESULT_NORMAL);
                break;
            default: break;
        }
    }

    if (s_state != FW_MODE_ACTIVE) return;

    if (++s_hbCounter >= 500000UL)
    {
        uint8_t p[8];
        s_hbCounter = 0;
        p[0] = (uint8_t)(10U + address);   /* bu id */
        p[1] = 0x01U;
        p[2] = (uint8_t)(APP_VERSION & 0xFFU);
        p[3] = (uint8_t)((APP_VERSION >> 8) & 0xFFU);
        p[4] = 'F'; p[5] = 'W'; p[6] = 'M'; p[7] = 'B';
        sendMCANA(HEARTBEAT_FW_MODE_CAN_ID, p, 8U);
    }
}

void FwMode_sendBootStatus(void)
{
    sendResult(FW_RESULT_BOOTED);
}

/* ── Internal ────────────────────────────────────────────────── */

static void enterMode(void)
{
    if (s_state == FW_MODE_ACTIVE) return;
    buBoardSuspendNormalMode();
    s_state = FW_MODE_ACTIVE;
    s_hbCounter = 0;
}

static void exitMode(void)
{
    if (s_state == FW_MODE_NORMAL) return;
    s_state = FW_MODE_NORMAL;
    buBoardResumeNormalMode();
}

static void sendResult(uint8_t status)
{
    uint8_t p[12];
    uint32_t crc = computeCRC32Image();
    uint16_t canId = (uint16_t)(10U + address);
    memset(p, 0, sizeof(p));
    p[0]  = RESP_FW_RESULT;
    p[1]  = (uint8_t)(10U + address);
    p[2]  = status;
    p[3]  = (uint8_t)(APP_VERSION & 0xFFU);
    p[4]  = (uint8_t)((APP_VERSION >> 8) & 0xFFU);
    p[5]  = (uint8_t)(crc & 0xFFU);
    p[6]  = (uint8_t)((crc >> 8) & 0xFFU);
    p[7]  = (uint8_t)((crc >> 16) & 0xFFU);
    p[8]  = (uint8_t)((crc >> 24) & 0xFFU);
    sendMCANA(canId, p, 12U);
}

/* CRC32 (zlib / IEEE 802.3 — poly 0xEDB88320, reflected) over the
 * installed app image only.  Reads the installed image size out of
 * Bank 3 sector 0 word 2..3 (written by the boot manager when it
 * marks the flag FW_FLAG_INSTALLED after a successful copy).  Falls
 * back to APP_IMAGE_MAX_SIZE on a JTAG-flashed board where no
 * installed-size record exists. */
static uint32_t computeCRC32Image(void)
{
    volatile uint16_t *flag = (volatile uint16_t *)FW_BANK3_FLAG_ADDR;
    uint32_t imageBytes;
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t numWords;
    volatile uint16_t *wp = (volatile uint16_t *)APP_IMAGE_START;
    uint32_t i, j;
    uint16_t b;

    if (flag[0] == FW_FLAG_INSTALLED)
    {
        imageBytes = (uint32_t)flag[2] | ((uint32_t)flag[3] << 16);
        if ((imageBytes == 0U) || (imageBytes > APP_IMAGE_MAX_SIZE))
        {
            imageBytes = APP_IMAGE_MAX_SIZE;
        }
    }
    else
    {
        imageBytes = APP_IMAGE_MAX_SIZE;
    }

    numWords = (imageBytes + 1U) / 2U;
    for (i = 0U; i < numWords; i++)
    {
        uint16_t w = wp[i];
        for (b = 0U; b < 2U; b++)
        {
            uint8_t byte = (b == 0U) ? (uint8_t)(w & 0xFFU)
                                      : (uint8_t)((w >> 8) & 0xFFU);
            crc ^= byte;
            for (j = 0U; j < 8U; j++)
            {
                if (crc & 1UL) crc = (crc >> 1) ^ 0xEDB88320UL;
                else           crc >>= 1;
            }
        }
    }
    return ~crc;
}

/* DLC encoding for CAN-FD: 0..8 → length, 9 → DLC9 (12 bytes),
 * A → 16, B → 20, C → 24, D → 32, E → 48, F → 64. */
static uint8_t dlcForLen(uint16_t len)
{
    if (len <= 8U)  return (uint8_t)len;
    if (len <= 12U) return 0x9U;
    if (len <= 16U) return 0xAU;
    if (len <= 20U) return 0xBU;
    if (len <= 24U) return 0xCU;
    if (len <= 32U) return 0xDU;
    if (len <= 48U) return 0xEU;
    return 0xFU;
}

static void sendMCANA(uint16_t canId, const uint8_t *payload, uint16_t len)
{
    MCAN_TxBufElement tx;
    volatile uint32_t t;
    uint16_t i;
    memset(&tx, 0, sizeof(tx));
    tx.id  = ((uint32_t)canId) << 18U;
    tx.dlc = dlcForLen(len);
    tx.brs = 1U; tx.fdf = 1U; tx.efc = 1U;
    for (i = 0U; i < len && i < 64U; i++) tx.data[i] = payload[i];
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 4U, &tx);
    MCAN_txBufAddReq(CAN_BU_BASE, 4U);
    for (t = 0U; t < 500000U; t++)
        if (MCAN_getTxBufReqPend(CAN_BU_BASE) == 0U) break;
}
