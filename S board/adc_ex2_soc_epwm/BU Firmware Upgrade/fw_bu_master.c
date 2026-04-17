/*
 * fw_bu_master.c
 *
 *  Streams the BU firmware image staged in S-Board flash Bank 1 to a
 *  BU board over MCANA, then asks it to verify and activate.
 *
 *  Time budget loosely matches the OTA plan: a 38 KB image becomes
 *  ~634 60-byte frames; ACK after every 16 frames; 500 ms ACK timeout
 *  with 3 retries; 5 s timeout for prepare/verify; 10 s for boot.
 */

#include "fw_bu_master.h"
#include "fw_bu_image_rx.h"
#include "can_driver.h"
#include "timer_driver.h"
#include <string.h>

/* ── Master state ─────────────────────────────────────────────── */
static FW_BuMasterState  fwBuM_state    = FW_BU_M_IDLE;
static FW_BuMasterError  fwBuM_error    = FW_BU_M_ERR_NONE;
static uint8_t           fwBuM_target   = 0xFFU;

/* ── Streaming progress ───────────────────────────────────────── */
static uint32_t          fwBuM_readAddr;       /* current Bank 1 word ptr */
static uint32_t          fwBuM_bytesSent;
static uint32_t          fwBuM_imageSize;
static uint32_t          fwBuM_imageCRC;
static uint32_t          fwBuM_imageVer;
static uint16_t          fwBuM_seq;
static uint16_t          fwBuM_burstStartSeq;
static uint16_t          fwBuM_burstCount;
static uint16_t          fwBuM_retries;

/* ── Response latch (set by ISR, consumed by main loop) ───────── */
static volatile uint8_t  fwBuM_respCode = 0U;
static volatile uint16_t fwBuM_respSeq  = 0U;
static volatile uint8_t  fwBuM_respBoard = 0U;
static volatile uint8_t  fwBuM_respPending = 0U;

/* Coarse millisecond clock — defined in timer_driver.c and driven by
 * CPU Timer 1 at 1 kHz (see initMsTick in timer_driver.c). Monotonic,
 * non-resettable, wraps at ~49 days. */
extern volatile uint32_t g_systemTickMs;

static uint32_t          fwBuM_phaseStartMs;

/* Phase timeouts come from fw_upgrade_config.h via the .h include. */
#define FW_BU_M_TMO_PREPARE_MS    FW_BU_TMO_PREPARE_MS
#define FW_BU_M_TMO_BURST_MS      FW_BU_TMO_BURST_MS
#define FW_BU_M_TMO_VERIFY_MS     FW_BU_TMO_VERIFY_MS
#define FW_BU_M_TMO_BOOT_MS       FW_BU_TMO_BOOT_MS

/* ── Forward declarations ─────────────────────────────────────── */
static void     FW_BuM_sendCommand(uint8_t cmd, const uint8_t *extra,
                                   uint16_t extraLen);
static void     FW_BuM_sendDataFrame(uint16_t seq);
static void     FW_BuM_sendHeader(void);
static uint16_t FW_BuM_consumeResponse(uint8_t *outCode, uint16_t *outSeq);
static void     FW_BuM_finishWith(FW_BuMasterState st, FW_BuMasterError err);
static uint32_t FW_BuM_now(void);
static uint32_t FW_BuM_elapsed(void);
static void     FW_BuM_resetPhase(void);
static void     FW_BuM_streamBurst(void);

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════ */
void FW_BuMaster_init(void)
{
    fwBuM_state       = FW_BU_M_IDLE;
    fwBuM_error       = FW_BU_M_ERR_NONE;
    fwBuM_respPending = 0U;
}

uint16_t FW_BuMaster_start(uint8_t boardId)
{
    if (fwBuM_state != FW_BU_M_IDLE && fwBuM_state != FW_BU_M_DONE &&
        fwBuM_state != FW_BU_M_FAILED)
    {
        return 1U;   /* busy */
    }

    if (FW_BuImageRx_getState() != FW_BU_RX_READY)
    {
        fwBuM_error = FW_BU_M_ERR_NO_IMAGE;
        return 2U;   /* nothing staged */
    }

    const FW_BuImageInfo *info = FW_BuImageRx_getImageInfo();
    fwBuM_target        = boardId;
    fwBuM_imageSize     = info->imageSize;
    fwBuM_imageCRC      = info->imageCRC;
    fwBuM_imageVer      = info->version;
    fwBuM_readAddr      = FW_BANK1_START;
    fwBuM_bytesSent     = 0U;
    fwBuM_seq           = 0U;
    fwBuM_burstStartSeq = 0U;
    fwBuM_burstCount    = 0U;
    fwBuM_retries       = 0U;
    fwBuM_respPending   = 0U;
    fwBuM_error         = FW_BU_M_ERR_NONE;

    /* Send PREPARE, wait for ACK in process(). */
    FW_BuM_sendCommand(BU_CMD_FW_PREPARE, NULL, 0U);
    fwBuM_state = FW_BU_M_PREPARING;
    FW_BuM_resetPhase();
    return 0U;
}

void FW_BuMaster_abort(void)
{
    FW_BuM_sendCommand(BU_CMD_FW_ABORT, NULL, 0U);
    FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_NAK);
}

FW_BuMasterState FW_BuMaster_getState(void)  { return fwBuM_state; }
FW_BuMasterError FW_BuMaster_getError(void)  { return fwBuM_error; }

uint8_t  FW_BuMaster_getTargetId(void)
{
    return (fwBuM_state == FW_BU_M_IDLE) ? 0xFFU : fwBuM_target;
}
uint32_t FW_BuMaster_getBytesSent(void)      { return fwBuM_bytesSent; }
uint32_t FW_BuMaster_getImageSize(void)      { return fwBuM_imageSize; }

/* Bulk-sweep tracking — populated by a future FW_BuMaster_startAll()
 * (phase 9). Until then the bitmap is always zero and the last-
 * completed id is 0xFF. */
static uint16_t fwBuM_bulkBitmap       = 0U;
static uint8_t  fwBuM_lastCompletedId  = 0xFFU;

uint16_t FW_BuMaster_getBulkBitmap(void)     { return fwBuM_bulkBitmap; }
uint8_t  FW_BuMaster_getLastCompleted(void)  { return fwBuM_lastCompletedId; }

/* ═══════════════════════════════════════════════════════════════
 *  STATUS REPLY — sent on FW_BU_STATUS_REPLY_ID (0x04) in response to
 *  CMD_BU_FW_STATUS_REQUEST (0x0F) from the M-Board.
 *
 *  Frame layout (bytes 0..14, rest padded with zero):
 *    [0]      echo of CMD_BU_FW_STATUS_REQUEST
 *    [1]      current target BU id (0xFF if idle)
 *    [2]      master state      (FW_BuMasterState)
 *    [3]      master error code (FW_BuMasterError)
 *    [4..7]   bytes_sent   u32 LE
 *    [8..11]  image_size   u32 LE
 *    [12]     last completed BU id (bulk sweep)
 *    [13..14] bulk bitmap (11..22 → bits 0..11)
 * ═══════════════════════════════════════════════════════════════ */
void FW_BuMaster_sendStatusReply(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)FW_BU_STATUS_REPLY_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;       /* 64 bytes CAN-FD */
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xDEU;

    TxMsg.data[0]  = CMD_BU_FW_STATUS_REQUEST;
    TxMsg.data[1]  = FW_BuMaster_getTargetId();
    TxMsg.data[2]  = (uint8_t)fwBuM_state;
    TxMsg.data[3]  = (uint8_t)fwBuM_error;

    uint32_t bytes = fwBuM_bytesSent;
    TxMsg.data[4]  = (uint8_t)(bytes       & 0xFFU);
    TxMsg.data[5]  = (uint8_t)((bytes >>  8) & 0xFFU);
    TxMsg.data[6]  = (uint8_t)((bytes >> 16) & 0xFFU);
    TxMsg.data[7]  = (uint8_t)((bytes >> 24) & 0xFFU);

    uint32_t size = fwBuM_imageSize;
    TxMsg.data[8]  = (uint8_t)(size        & 0xFFU);
    TxMsg.data[9]  = (uint8_t)((size  >> 8) & 0xFFU);
    TxMsg.data[10] = (uint8_t)((size  >> 16)& 0xFFU);
    TxMsg.data[11] = (uint8_t)((size  >> 24)& 0xFFU);

    TxMsg.data[12] = fwBuM_lastCompletedId;
    TxMsg.data[13] = (uint8_t)(fwBuM_bulkBitmap       & 0xFFU);
    TxMsg.data[14] = (uint8_t)((fwBuM_bulkBitmap >> 8) & 0xFFU);

    /* OTA status reply goes to M-Board via MCANB */
    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 5U, &TxMsg);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 5U);
    canTxWaitComplete(CAN_MBOARD_BASE);
}

void FW_BuMaster_isrOnResponse(const MCAN_RxBufElement *rxElem)
{
    /* Frame layout: data[0]=respCode, data[1]=boardId,
     * data[2..3]=seq (LE u16). */
    fwBuM_respCode    = rxElem->data[0];
    fwBuM_respBoard   = rxElem->data[1];
    fwBuM_respSeq     = (uint16_t)rxElem->data[2] |
                        ((uint16_t)rxElem->data[3] << 8);
    fwBuM_respPending = 1U;
}

/* ═══════════════════════════════════════════════════════════════
 *  STATE MACHINE
 * ═══════════════════════════════════════════════════════════════ */
FW_BuMasterState FW_BuMaster_process(void)
{
    uint8_t  rcode = 0U;
    uint16_t rseq  = 0U;
    uint16_t haveResp = FW_BuM_consumeResponse(&rcode, &rseq);

    switch (fwBuM_state)
    {
        case FW_BU_M_PREPARING:
            if (haveResp)
            {
                if (rcode == BU_RESP_FW_ACK)
                {
                    fwBuM_state = FW_BU_M_SEND_HEADER;
                    FW_BuM_sendHeader();
                    FW_BuM_resetPhase();
                }
                else
                {
                    FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_NAK);
                }
            }
            else if (FW_BuM_elapsed() > FW_BU_M_TMO_PREPARE_MS)
            {
                FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_TIMEOUT);
            }
            break;

        case FW_BU_M_SEND_HEADER:
            if (haveResp)
            {
                if (rcode == BU_RESP_FW_ACK)
                {
                    fwBuM_state = FW_BU_M_SENDING_DATA;
                    FW_BuM_resetPhase();
                    FW_BuM_streamBurst();
                }
                else
                {
                    FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_NAK);
                }
            }
            else if (FW_BuM_elapsed() > FW_BU_M_TMO_PREPARE_MS)
            {
                FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_TIMEOUT);
            }
            break;

        case FW_BU_M_SENDING_DATA:
            if (haveResp)
            {
                if (rcode == BU_RESP_FW_ACK)
                {
                    /* BU acknowledged the burst window. Reset retries
                     * and either send the next burst or, if the entire
                     * image has been sent, request verification. */
                    fwBuM_retries = 0U;
                    if (fwBuM_bytesSent >= fwBuM_imageSize)
                    {
                        fwBuM_state = FW_BU_M_VERIFYING;
                        FW_BuM_sendCommand(BU_CMD_FW_VERIFY, NULL, 0U);
                        FW_BuM_resetPhase();
                    }
                    else
                    {
                        FW_BuM_resetPhase();
                        FW_BuM_streamBurst();
                    }
                }
                else
                {
                    FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_NAK);
                }
            }
            else if (FW_BuM_elapsed() > FW_BU_M_TMO_BURST_MS)
            {
                if (++fwBuM_retries > FW_BU_MASTER_RETRIES)
                {
                    FW_BuM_finishWith(FW_BU_M_FAILED,
                                      FW_BU_M_ERR_RETRIES_EXHAUSTED);
                }
                else
                {
                    /* Rewind to the start of the burst and retransmit. */
                    uint32_t bytesPerBurst =
                        (uint32_t)FW_BU_MASTER_BURST * FW_BU_MASTER_PAYLOAD;
                    uint32_t rewindBytes = (fwBuM_burstCount > 0U) ?
                        ((uint32_t)fwBuM_burstCount * FW_BU_MASTER_PAYLOAD) :
                        bytesPerBurst;
                    if (rewindBytes > fwBuM_bytesSent)
                        rewindBytes = fwBuM_bytesSent;

                    fwBuM_bytesSent -= rewindBytes;
                    fwBuM_readAddr  -= (rewindBytes / 2U);
                    fwBuM_seq        = fwBuM_burstStartSeq;
                    FW_BuM_resetPhase();
                    FW_BuM_streamBurst();
                }
            }
            break;

        case FW_BU_M_VERIFYING:
            if (haveResp)
            {
                if (rcode == BU_RESP_FW_VERIFY_PASS)
                {
                    FW_BuM_sendCommand(BU_CMD_FW_ACTIVATE, NULL, 0U);
                    fwBuM_state = FW_BU_M_ACTIVATING;
                    FW_BuM_resetPhase();
                }
                else
                {
                    FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_VERIFY);
                }
            }
            else if (FW_BuM_elapsed() > FW_BU_M_TMO_VERIFY_MS)
            {
                FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_TIMEOUT);
            }
            break;

        case FW_BU_M_ACTIVATING:
            if (haveResp && rcode == BU_RESP_FW_BOOT_OK)
            {
                fwBuM_state = FW_BU_M_DONE;
                fwBuM_error = FW_BU_M_ERR_NONE;
            }
            else if (FW_BuM_elapsed() > FW_BU_M_TMO_BOOT_MS)
            {
                FW_BuM_finishWith(FW_BU_M_FAILED, FW_BU_M_ERR_TIMEOUT);
            }
            break;

        case FW_BU_M_IDLE:
        case FW_BU_M_DONE:
        case FW_BU_M_FAILED:
        default:
            break;
    }

    return fwBuM_state;
}

/* ═══════════════════════════════════════════════════════════════
 *  HELPERS
 * ═══════════════════════════════════════════════════════════════ */
static uint32_t FW_BuM_now(void)        { return g_systemTickMs; }
static void     FW_BuM_resetPhase(void) { fwBuM_phaseStartMs = FW_BuM_now(); }
static uint32_t FW_BuM_elapsed(void)    { return FW_BuM_now() - fwBuM_phaseStartMs; }

static uint16_t FW_BuM_consumeResponse(uint8_t *outCode, uint16_t *outSeq)
{
    if (!fwBuM_respPending)
        return 0U;

    /* Filter on board id — ignore replies from BUs we are not
     * currently targeting. Broadcast mode (fwBuM_target == 0xFF)
     * bypasses the filter: any BU on the bus that responds counts,
     * because the master is talking to "all of them" via broadcast. */
    if (fwBuM_target != BU_BROADCAST_ID &&
        fwBuM_respBoard != fwBuM_target &&
        fwBuM_respBoard != BU_BROADCAST_ID)
    {
        fwBuM_respPending = 0U;
        return 0U;
    }
    *outCode = fwBuM_respCode;
    *outSeq  = fwBuM_respSeq;
    fwBuM_respPending = 0U;
    return 1U;
}

static void FW_BuM_finishWith(FW_BuMasterState st, FW_BuMasterError err)
{
    fwBuM_state = st;
    fwBuM_error = err;
}

/* Send a small command frame (no payload, or with `extra` bytes
 * appended after the 4-byte header). */
static void FW_BuM_sendCommand(uint8_t cmd, const uint8_t *extra,
                               uint16_t extraLen)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)FW_BU_TX_CMD_CAN_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;            /* 64-byte CAN-FD frame */
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xCDU;

    TxMsg.data[0] = cmd;
    TxMsg.data[1] = fwBuM_target;
    TxMsg.data[2] = (uint8_t)(fwBuM_seq & 0xFFU);
    TxMsg.data[3] = (uint8_t)((fwBuM_seq >> 8) & 0xFFU);

    if (extra != NULL)
    {
        uint16_t i;
        for (i = 0; i < extraLen && (uint16_t)(4U + i) < 64U; i++)
            TxMsg.data[4U + i] = extra[i];
    }

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 4U, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 4U);
    canTxWaitComplete(CAN_BU_BASE);
}

static void FW_BuM_sendHeader(void)
{
    uint8_t hdr[16];
    hdr[0]  = (uint8_t)(fwBuM_imageSize        & 0xFFU);
    hdr[1]  = (uint8_t)((fwBuM_imageSize >> 8) & 0xFFU);
    hdr[2]  = (uint8_t)((fwBuM_imageSize >> 16)& 0xFFU);
    hdr[3]  = (uint8_t)((fwBuM_imageSize >> 24)& 0xFFU);
    hdr[4]  = (uint8_t)(fwBuM_imageCRC        & 0xFFU);
    hdr[5]  = (uint8_t)((fwBuM_imageCRC >> 8) & 0xFFU);
    hdr[6]  = (uint8_t)((fwBuM_imageCRC >> 16)& 0xFFU);
    hdr[7]  = (uint8_t)((fwBuM_imageCRC >> 24)& 0xFFU);
    hdr[8]  = (uint8_t)(fwBuM_imageVer        & 0xFFU);
    hdr[9]  = (uint8_t)((fwBuM_imageVer >> 8) & 0xFFU);
    hdr[10] = (uint8_t)((fwBuM_imageVer >> 16)& 0xFFU);
    hdr[11] = (uint8_t)((fwBuM_imageVer >> 24)& 0xFFU);
    /* totalFrames computed by receiver from imageSize */
    hdr[12] = hdr[13] = hdr[14] = hdr[15] = 0U;

    FW_BuM_sendCommand(BU_CMD_FW_HEADER, hdr, sizeof(hdr));
}

/* Send one data frame: 60 bytes of payload pulled from Bank 1. */
static void FW_BuM_sendDataFrame(uint16_t seq)
{
    MCAN_TxBufElement TxMsg;
    uint16_t i;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)FW_BU_TX_DATA_CAN_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;          /* CAN-FD 64-byte data frame */
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xCEU;
    (void)seq;               /* no per-frame seq in raw data frames */

    /* RAW 64 bytes — no header. Read 32 words from Bank 1 starting
     * at the current read address. Pad with 0xFF if we'd run past
     * the staged image so the receiver still sees a full-length
     * frame. (The receiver only CRCs imageSize bytes, so trailing
     * pad bytes are harmless.) */
    volatile uint16_t *src = (volatile uint16_t *)fwBuM_readAddr;
    for (i = 0; i < 32U; i++)
    {
        uint32_t bytePos = fwBuM_bytesSent + (uint32_t)(i * 2U);
        uint16_t w = (bytePos < fwBuM_imageSize) ? src[i] : 0xFFFFU;
        TxMsg.data[(i * 2U)]      = (uint8_t)(w & 0xFFU);
        TxMsg.data[(i * 2U) + 1U] = (uint8_t)((w >> 8) & 0xFFU);
    }

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 4U, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 4U);
    canTxWaitComplete(CAN_BU_BASE);

    fwBuM_readAddr  += 32U;                  /* advance by 32 words */
    fwBuM_bytesSent += FW_BU_MASTER_PAYLOAD; /* 64 bytes */
}

/* Stream up to FW_BU_MASTER_BURST frames, then wait for an ACK. */
static void FW_BuM_streamBurst(void)
{
    fwBuM_burstStartSeq = fwBuM_seq;
    fwBuM_burstCount    = 0U;

    while (fwBuM_burstCount < FW_BU_MASTER_BURST &&
           fwBuM_bytesSent  < fwBuM_imageSize)
    {
        FW_BuM_sendDataFrame(fwBuM_seq);
        fwBuM_seq++;
        fwBuM_burstCount++;

        /* Pace the burst so frames don't pile up in the BU's
         * dedicated RX buffer faster than the BU's flash-bound ISR
         * can drain them. See FW_BU_INTER_FRAME_DELAY_US in
         * fw_upgrade_config.h for the reasoning. */
#if defined(FW_BU_INTER_FRAME_DELAY_US) && (FW_BU_INTER_FRAME_DELAY_US > 0)
        DEVICE_DELAY_US(FW_BU_INTER_FRAME_DELAY_US);
#endif
    }
}
