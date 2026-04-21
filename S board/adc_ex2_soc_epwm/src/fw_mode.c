/*
 * fw_mode.c — S-Board FW Update Mode control (implementation)
 */

#include "src/fw_mode.h"
#include "driverlib.h"
#include "device.h"
#include "can_config.h"
#include <string.h>

/* ── Module state ─────────────────────────────────────────────── */
static FwModeState       s_state = FW_MODE_NORMAL;
static uint32_t          s_hbCounter = 0;       /* main-loop ticks since last heartbeat */
static FwBuResult        s_buResults[MAX_BU_BOARDS];
static volatile uint8_t  s_pendingCmd = 0;      /* set by ISR, drained by tick */

/* ── Forward decls from main translation unit ─────────────────── */
extern void sBoardSuspendNormalMode(void);
extern void sBoardResumeNormalMode(void);

/* CAN-FD DLC encoding (>8 bytes uses special codes). */
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

/* ── Local helpers ────────────────────────────────────────────── */
static uint32_t computeCRC32Image(void);
static void     sendMCANB(uint16_t canId, const uint8_t *payload, uint16_t len);
static void     sendMCANA_BU(uint16_t canId, const uint8_t *payload, uint16_t len);
static void     broadcastToBUs(uint8_t cmd);
static void     sendStatusSummary(void);
static void     enterMode(void);
static void     exitMode(void);
static void     pollAndReportSummary(void);

/* ══════════════════════════════════════════════════════════════
 *  Public API
 * ══════════════════════════════════════════════════════════════ */
void FwMode_init(void)
{
    s_state     = FW_MODE_NORMAL;
    s_hbCounter = 0;
    memset(s_buResults, 0, sizeof(s_buResults));
}

FwModeState FwMode_getState(void) { return s_state; }

void FwMode_tick(void)
{
    /* Drain any pending command stashed by the ISR */
    if (s_pendingCmd != 0U)
    {
        uint8_t cmd = s_pendingCmd;
        s_pendingCmd = 0U;
        switch (cmd)
        {
            case CMD_ENTER_FW_MODE:   enterMode();            break;
            case CMD_EXIT_FW_MODE:    exitMode();             break;
            case CMD_FW_STATUS_REQ:   pollAndReportSummary(); break;
            default: break;
        }
    }

    if (s_state != FW_MODE_ACTIVE) return;

    /* Rough 1 Hz heartbeat — main loop runs at ~hundreds of kHz when
     * nothing else is blocking, so this gets us a usable cadence
     * without a dedicated timer. Pick a threshold empirically. */
    if (++s_hbCounter >= 500000UL)
    {
        uint8_t p[8];
        s_hbCounter = 0;
        p[0] = 0x00U;                       /* S-Board ID marker */
        p[1] = 0x01U;                       /* fw_mode flag */
        p[2] = (uint8_t)(APP_VERSION & 0xFFU);
        p[3] = (uint8_t)((APP_VERSION >> 8) & 0xFFU);
        p[4] = 'F'; p[5] = 'W'; p[6] = 'M'; p[7] = 'S';
        sendMCANB(HEARTBEAT_FW_MODE_CAN_ID, p, 8U);
    }
}

uint8_t FwMode_isrOnCommand(const uint8_t *data)
{
    uint8_t cmd = data[0];
    if (cmd == CMD_ENTER_FW_MODE ||
        cmd == CMD_EXIT_FW_MODE  ||
        cmd == CMD_FW_STATUS_REQ)
    {
        s_pendingCmd = cmd;
        return 1U;
    }
    return 0U;
}

/* ── ENTER ───────────────────────────────────────────────────── */
static void enterMode(void)
{
    if (s_state == FW_MODE_ACTIVE) return;     /* idempotent */

    sBoardSuspendNormalMode();
    s_state = FW_MODE_ACTIVE;
    s_hbCounter = 0;
    memset(s_buResults, 0, sizeof(s_buResults));

    broadcastToBUs(BU_CMD_ENTER_FW_MODE);
}

/* ── EXIT ────────────────────────────────────────────────────── */
static void exitMode(void)
{
    if (s_state == FW_MODE_NORMAL) return;     /* idempotent */

    broadcastToBUs(BU_CMD_EXIT_FW_MODE);

    s_state = FW_MODE_NORMAL;
    sBoardResumeNormalMode();
}

/* ── STATUS REQUEST (pull) ───────────────────────────────────── */
static void pollAndReportSummary(void)
{
    /* Kick every BU for a fresh result — they'll reply async on MCANA.
     * Replies arrive in the MCANA ISR and land in s_buResults via
     * FwMode_recordBuResult().  Wait briefly, then emit. */
    broadcastToBUs(BU_CMD_FW_STATUS_REQ);

    {
        volatile uint32_t t;
        for (t = 0U; t < 5000000U; t++) { /* ~tens of ms */ }
    }

    sendStatusSummary();
}

/* ── BU result sink (called from MCANA RX dispatch) ──────────── */
void FwMode_recordBuResult(const uint8_t *data)
{
    /* RESP_FW_RESULT payload:
     *   [0] 0x41 (cmd)
     *   [1] bu_id       (10..10+MAX_BU-1)
     *   [2] status      (FW_RESULT_*)
     *   [3..4] version  (LE)
     *   [5..8] crc32    (LE)
     */
    uint8_t bu_id  = data[1];
    uint16_t idx;
    if (bu_id < FIRST_BU_ID) return;
    idx = (uint16_t)(bu_id - FIRST_BU_ID);
    if (idx >= MAX_BU_BOARDS) return;

    s_buResults[idx].valid   = 1U;
    s_buResults[idx].bu_id   = bu_id;
    s_buResults[idx].status  = data[2];
    s_buResults[idx].version = (uint16_t)data[3] |
                               ((uint16_t)data[4] << 8);
    s_buResults[idx].crc     = (uint32_t)data[5] |
                               ((uint32_t)data[6] << 8) |
                               ((uint32_t)data[7] << 16) |
                               ((uint32_t)data[8] << 24);
}

/* ── Boot-time status push ───────────────────────────────────── */
void FwMode_sendBootStatus(void)
{
    /* Matches the BU board's RESP_FW_RESULT frame layout so the
     * M-Board can parse S and BU results with the same code. */
    uint8_t p[12];
    uint32_t crc = computeCRC32Image();
    memset(p, 0, sizeof(p));
    p[0] = RESP_FW_RESULT;
    p[1] = 0x00U;                                 /* S-Board marker (id 0) */
    p[2] = FW_RESULT_BOOTED;
    p[3] = (uint8_t)(APP_VERSION & 0xFFU);
    p[4] = (uint8_t)((APP_VERSION >> 8) & 0xFFU);
    p[5] = (uint8_t)(crc & 0xFFU);
    p[6] = (uint8_t)((crc >> 8) & 0xFFU);
    p[7] = (uint8_t)((crc >> 16) & 0xFFU);
    p[8] = (uint8_t)((crc >> 24) & 0xFFU);
    sendMCANB(RESP_FW_RESULT, p, 12U);
}

/* ══════════════════════════════════════════════════════════════
 *  Local helpers
 * ══════════════════════════════════════════════════════════════ */

/* CRC32 (zlib / IEEE 802.3 — poly 0xEDB88320, reflected) over
 * APP_IMAGE_START for APP_IMAGE_MAX_SIZE bytes. Stable across boots
 * because the app region is immutable once flashed. */
static uint32_t computeCRC32Image(void)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t numWords = APP_IMAGE_MAX_SIZE / 2U;
    volatile uint16_t *wp = (volatile uint16_t *)APP_IMAGE_START;
    uint32_t i, j;
    uint16_t b;
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

static void sendMCANB(uint16_t canId, const uint8_t *payload, uint16_t len)
{
    MCAN_TxBufElement tx;
    volatile uint32_t t;
    uint16_t i;
    memset(&tx, 0, sizeof(tx));
    tx.id  = ((uint32_t)canId) << 18U;
    tx.dlc = dlcForLen(len);
    tx.brs = 1U; tx.fdf = 1U; tx.efc = 1U;
    for (i = 0U; i < len && i < 64U; i++) tx.data[i] = payload[i];
    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 0U, &tx);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 0U);
    for (t = 0U; t < 500000U; t++)
        if (MCAN_getTxBufReqPend(CAN_MBOARD_BASE) == 0U) break;
}

static void sendMCANA_BU(uint16_t canId, const uint8_t *payload, uint16_t len)
{
    MCAN_TxBufElement tx;
    volatile uint32_t t;
    uint16_t i;
    memset(&tx, 0, sizeof(tx));
    tx.id  = ((uint32_t)canId) << 18U;
    tx.dlc = (len <= 8U) ? len : 0xFU;
    tx.brs = 1U; tx.fdf = 1U; tx.efc = 1U;
    for (i = 0U; i < len && i < 64U; i++) tx.data[i] = payload[i];
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 0U, &tx);
    MCAN_txBufAddReq(CAN_BU_BASE, 0U);
    for (t = 0U; t < 500000U; t++)
        if (MCAN_getTxBufReqPend(CAN_BU_BASE) == 0U) break;
}

static void broadcastToBUs(uint8_t cmd)
{
    uint8_t p[8];
    memset(p, 0, 8U);
    p[0] = cmd;
    sendMCANA_BU(BU_FW_BROADCAST_CAN_ID, p, 8U);
}

static void sendStatusSummary(void)
{
    uint8_t  p[8];
    uint16_t i;
    uint8_t  num_bu   = 0;
    uint8_t  num_ok   = 0;
    uint8_t  num_fail = 0;
    uint16_t bitmap   = 0;

    for (i = 0U; i < MAX_BU_BOARDS; i++)
    {
        if (s_buResults[i].valid)
        {
            num_bu++;
            if (s_buResults[i].status == FW_RESULT_BOOTED ||
                s_buResults[i].status == FW_RESULT_NORMAL)
            {
                num_ok++;
                bitmap |= (uint16_t)(1U << i);
            }
            else
            {
                num_fail++;
            }
        }
    }

    memset(p, 0, 8U);
    p[0] = FwMode_isActive() ? 0x01U : 0x00U;  /* S mode flag */
    p[1] = num_bu;
    p[2] = num_ok;
    p[3] = num_fail;
    p[4] = (uint8_t)(APP_VERSION & 0xFFU);
    p[5] = (uint8_t)((APP_VERSION >> 8) & 0xFFU);
    p[6] = (uint8_t)(bitmap & 0xFFU);
    p[7] = (uint8_t)((bitmap >> 8) & 0xFFU);
    sendMCANB(RESP_FW_SUMMARY, p, 8U);
}
