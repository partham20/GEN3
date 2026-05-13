/*
 * bu_health.c -- BU fleet health monitor (S-Board side).
 *
 * See bu_health.h for the wire-format contract.
 */

#include "src/bu_health.h"
#include "driverlib.h"
#include "can_config.h"          /* CAN_MBOARD_BASE */
#include "timer_driver.h"        /* g_systemTickMs */
#include "bu_board.h"            /* duplicateIDFound, activeBUMask */
#include <string.h>

/* Per-slot health record.  Slot index i <-> BU board id (FIRST_BU_ID+i). */
typedef struct
{
    uint32_t lastSeenMs;        /* g_systemTickMs of last runtime frame */
    uint8_t  version;           /* last reported version byte */
    uint8_t  everSeen;          /* set to 1 the first time we hear from this BU */
} BuHealthSlot_t;

static BuHealthSlot_t s_slots[MAX_BU_BOARDS];
static uint32_t       s_lastReportMs = 0U;

void BuHealth_init(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_lastReportMs = 0U;
}

void BuHealth_onRuntimeFrame(uint32_t boardId, uint8_t version)
{
    uint16_t idx;
    if (boardId < FIRST_BU_ID) return;
    idx = (uint16_t)(boardId - FIRST_BU_ID);
    if (idx >= MAX_BU_BOARDS) return;

    s_slots[idx].lastSeenMs = g_systemTickMs;
    s_slots[idx].version    = version;
    s_slots[idx].everSeen   = 1U;
}

/* DLC encoding for CAN-FD: 0..8 -> length, 9 -> 12, A -> 16,
 * B -> 20, C -> 24, D -> 32, E -> 48, F -> 64. */
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

static void sendHealthFrame(const uint8_t *payload, uint16_t len)
{
    MCAN_TxBufElement tx;
    volatile uint32_t t;
    uint16_t i;

    memset(&tx, 0, sizeof(tx));
    tx.id  = ((uint32_t)FW_MODE_RESP_CAN_ID) << 18U;
    tx.dlc = dlcForLen(len);
    tx.brs = 1U; tx.fdf = 1U; tx.efc = 1U;
    for (i = 0U; i < len && i < 64U; i++) tx.data[i] = payload[i];

    /* Use buffer 1 -- buffer 0 is owned by fw_mode's sendMCANB. */
    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 1U, &tx);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 1U);
    for (t = 0U; t < 500000U; t++)
        if ((MCAN_getTxBufReqPend(CAN_MBOARD_BASE) & (1UL << 1U)) == 0U) break;
}

static uint8_t popcount16(uint16_t x)
{
    uint8_t c = 0U;
    while (x != 0U) { c = (uint8_t)(c + (x & 1U)); x = (uint16_t)(x >> 1); }
    return c;
}

void BuHealth_tick(void)
{
    uint32_t now = g_systemTickMs;
    uint16_t i;
    uint16_t presentBm = 0U;       /* ever-seen */
    uint16_t aliveBm   = 0U;       /* seen within BU_HEALTH_DEAD_MS */
    uint16_t mismatchBm = 0U;      /* version != APP_VERSION low byte */
    uint16_t duplicateBm = 0U;     /* duplicate CAN-ID detected during discovery */
    uint16_t deadBm    = 0U;       /* present but not alive */
    uint8_t  expectedVer = (uint8_t)(APP_VERSION & 0xFFU);

    /* Rate-limit: only emit one frame per BU_HEALTH_REPORT_MS. */
    if ((now - s_lastReportMs) < BU_HEALTH_REPORT_MS) return;
    s_lastReportMs = now;

    /* Per-slot evaluation. */
    for (i = 0U; i < MAX_BU_BOARDS; i++)
    {
        BuHealthSlot_t *sl = &s_slots[i];
        uint8_t bit = (uint8_t)(1U << i);

        if (sl->everSeen)
        {
            presentBm = (uint16_t)(presentBm | bit);
            if ((now - sl->lastSeenMs) < BU_HEALTH_DEAD_MS)
                aliveBm = (uint16_t)(aliveBm | bit);
            if (sl->version != expectedVer)
                mismatchBm = (uint16_t)(mismatchBm | bit);
        }
    }
    deadBm = (uint16_t)(presentBm & ~aliveBm);

    /* Duplicate CAN IDs: bu_board.c tracks the specific slots
     * that collided in duplicateIDMask (bit i = slot i collided). */
    duplicateBm = (uint16_t)(duplicateIDMask & 0xFFFFU);

    /* Build the payload (40 bytes -- DLC will round up to 48). */
    uint8_t p[48];
    memset(p, 0, sizeof(p));
    p[ 0] = RESP_BU_HEALTH;
    p[ 1] = popcount16(presentBm);
    p[ 2] = popcount16(aliveBm);
    p[ 3] = (uint8_t)(APP_VERSION & 0xFFU);
    p[ 4] = (uint8_t)((APP_VERSION >> 8) & 0xFFU);
    p[ 5] = (uint8_t)(((mismatchBm  != 0U) ? 0x01U : 0U) |
                       ((duplicateBm != 0U) ? 0x02U : 0U) |
                       ((deadBm      != 0U) ? 0x04U : 0U));
    p[ 6] = (uint8_t)(presentBm   & 0xFFU);
    p[ 7] = (uint8_t)((presentBm   >> 8) & 0xFFU);
    p[ 8] = (uint8_t)(aliveBm     & 0xFFU);
    p[ 9] = (uint8_t)((aliveBm     >> 8) & 0xFFU);
    p[10] = (uint8_t)(mismatchBm  & 0xFFU);
    p[11] = (uint8_t)((mismatchBm  >> 8) & 0xFFU);
    p[12] = (uint8_t)(duplicateBm & 0xFFU);
    p[13] = (uint8_t)((duplicateBm >> 8) & 0xFFU);
    p[14] = (uint8_t)(deadBm      & 0xFFU);
    p[15] = (uint8_t)((deadBm      >> 8) & 0xFFU);

    /* Per-slot detail. */
    for (i = 0U; i < MAX_BU_BOARDS; i++)
    {
        uint8_t bit = (uint8_t)(1U << i);
        uint8_t status;
        if ((duplicateBm & bit) != 0U)      status = 3U;
        else if ((deadBm   & bit) != 0U)    status = 2U;
        else if ((aliveBm  & bit) != 0U)    status = 1U;
        else                                status = 0U;
        p[16U + 2U * i]      = s_slots[i].version;
        p[16U + 2U * i + 1U] = status;
    }

    sendHealthFrame(p, 40U);
}
