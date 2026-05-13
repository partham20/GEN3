/*
 * fw_update_bu.c
 *
 *  BU board firmware-update receiver.
 *
 *  Listens for FW commands & data from the S-Board on MCANA, writes
 *  incoming bytes into Bank 2 (staging), verifies the CRC, then on
 *  ACTIVATE writes the boot flag into Bank 3 sector 0 and resets.
 *  The BU boot manager (loaded by JTAG, never touched by OTA) sees
 *  the flag at next boot, copies Bank 2 -> Bank 0 sectors 4..127,
 *  clears the flag, and jumps to the new app at 0x081000.
 *
 *  Symmetric counterpart: S-Board fw_bu_master.c.
 *
 *  IMPORTANT: this file uses the TI Flash API (Fapi_*). The BU
 *  project does NOT yet link against FAPI_F28P55x_EABI_v4.00.00.lib.
 *  Add the lib + headers (already present in the S-Board project
 *  under device/ and as a top-level .lib) before enabling this
 *  module in the build. Until then the file builds with
 *  BU_FW_FLASH_STUB defined and flash operations are no-ops so the
 *  CAN protocol can still be exercised.
 */

#include "fw_update_bu.h"
#include "../can_bu.h"        /* relative — this file lives in src/Firmware Upgrade/ */
#include <string.h>

/* FAPI is now linked into the BU project
 * (FAPI_F28P55x_EABI_v4.00.00.lib copied alongside flash_programming_f28p55x.h).
 * Real flash path is enabled by default. Build with -DBU_FW_FLASH_STUB=1
 * to fall back to the no-op stub for CAN-protocol-only bring-up. */
#ifndef BU_FW_FLASH_STUB
#define BU_FW_FLASH_STUB  0
#endif

#if !BU_FW_FLASH_STUB
#  include "FlashTech_F28P55x_C28x.h"
#  include "flash_programming_f28p55x.h"
#endif

/* ── State ────────────────────────────────────────────────────── */
static volatile BU_FwState fwState = BU_FW_IDLE;
static BU_FwImageInfo      fwInfo;
static uint8_t             fwMyBoardId = 0xFFU;

/* ── Streaming progress ───────────────────────────────────────── */
static uint32_t            fwWriteAddr;       /* next flash write addr */
static uint32_t            fwBytesReceived;
static uint32_t            fwFramesReceived;
static uint16_t            fwBurstCount;
static uint16_t            fwLastSeq;

/* ── Ring buffer (ISR producer, main-loop consumer) ───────────── */
/* Data frames carry RAW 64-byte firmware payloads with no header,
 * so the ring entry is just the payload. The "seq" field that used
 * to be parsed from data[2..3] is gone — the master tracks order
 * implicitly by stream position. */
typedef struct {
    uint8_t  payload[BU_FW_PAYLOAD_BYTES];
} BU_FwRingEntry;

static BU_FwRingEntry      fwRing[BU_FW_RX_RING_SIZE];
static volatile uint16_t   fwRingHead = 0;
static uint16_t            fwRingTail = 0;

/* ── Pending command (ISR -> main loop) ───────────────────────── */
static volatile uint8_t    fwPendingCmd = 0U;
static MCAN_RxBufElement   fwPendingMsg;

/* ── Local CRC32 (matches S-Board polynomial 0xEDB88320) ──────── */
static const uint32_t fwCrc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA,
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE,
    0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940,
    0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116,
    0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A,
    0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818,
    0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C,
    0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2,
    0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086,
    0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4,
    0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE,
    0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252,
    0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60,
    0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04,
    0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E,
    0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C,
    0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0,
    0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6,
    0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

/* ── Forward declarations ─────────────────────────────────────── */
static void     buFw_handlePrepare(void);
static void     buFw_handleHeader(void);
static void     buFw_handleVerify(void);
static void     buFw_handleActivate(void);
static void     buFw_handleAbort(void);
static void     buFw_writePayloadToFlash(const uint8_t *payload);
static uint32_t buFw_calculateCRC32(uint32_t startAddr, uint32_t numBytes);
static bool     buFw_eraseStagingBank(void);
static void     buFw_writeBootFlag(void);
static void     buFw_sendResponse(uint8_t respCode, uint16_t seq);
static bool     buFw_isAddressedToMe(const MCAN_RxBufElement *rxElem);

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════ */
void BU_Fw_init(uint8_t myBoardId)
{
    fwMyBoardId      = myBoardId;
    fwState          = BU_FW_IDLE;
    fwRingHead       = 0U;
    fwRingTail       = 0U;
    fwPendingCmd     = 0U;
    fwBytesReceived  = 0U;
    fwFramesReceived = 0U;
    fwBurstCount     = 0U;
    fwLastSeq        = 0U;
    fwWriteAddr      = BU_BANK2_START;
    memset(&fwInfo, 0, sizeof(fwInfo));
}

BU_FwState BU_Fw_getState(void) { return fwState; }
bool       BU_Fw_isBusy(void)   { return fwState != BU_FW_IDLE; }

void BU_Fw_isrOnFrame(uint32_t canId, const MCAN_RxBufElement *rxElem)
{
    if (canId == BU_FW_DATA_CAN_ID)
    {
        /* Data frames carry RAW firmware bytes — no header, no
         * per-frame target id. The target was already chosen when
         * we accepted PREPARE for ourselves, so any data frame on
         * 0x30 while we are in RECEIVING state belongs to us. */
        if (fwState != BU_FW_RECEIVING)
            return;

        uint16_t nextHead = (fwRingHead + 1U) & BU_FW_RX_RING_MASK;
        if (nextHead == fwRingTail)
            return;  /* ring full — drop, master will retransmit */

        memcpy(fwRing[fwRingHead].payload, rxElem->data,
               BU_FW_PAYLOAD_BYTES);
        fwRingHead = nextHead;
    }
    else if (canId == BU_FW_CMD_CAN_ID)
    {
        /* Command frames still carry a target id in data[1] —
         * filter them so other BUs on the bus don't react to a
         * PREPARE / HEADER / VERIFY / ACTIVATE / ABORT meant for
         * a different board. */
        if (!buFw_isAddressedToMe(rxElem))
            return;

        memcpy(&fwPendingMsg, rxElem, sizeof(MCAN_RxBufElement));
        fwPendingCmd = rxElem->data[0];
    }
}

BU_FwState BU_Fw_process(void)
{
    /* 1. Pending command? */
    uint8_t cmd = fwPendingCmd;
    if (cmd != 0U)
    {
        fwPendingCmd = 0U;
        switch (cmd)
        {
            case BU_CMD_FW_PREPARE:  buFw_handlePrepare();  break;
            case BU_CMD_FW_HEADER:   buFw_handleHeader();   break;
            case BU_CMD_FW_VERIFY:   buFw_handleVerify();   break;
            case BU_CMD_FW_ACTIVATE: buFw_handleActivate(); break;
            case BU_CMD_FW_ABORT:    buFw_handleAbort();    break;
            default: break;
        }
    }

    /* 2. Drain ring -> flash */
    if (fwState == BU_FW_RECEIVING)
    {
        while (fwRingTail != fwRingHead)
        {
            buFw_writePayloadToFlash(fwRing[fwRingTail].payload);
            fwRingTail = (fwRingTail + 1U) & BU_FW_RX_RING_MASK;
            /* Implicit sequence number — frame index in this session.
             * The master uses this in its burst-ACK matching. */
            fwLastSeq = (uint16_t)fwFramesReceived;

            fwFramesReceived++;
            fwBurstCount++;
            fwBytesReceived += BU_FW_PAYLOAD_BYTES;

            if (fwBurstCount >= BU_FW_BURST_SIZE)
            {
                buFw_sendResponse(BU_RESP_FW_ACK, fwLastSeq);
                fwBurstCount = 0U;
            }
        }

        /* Final partial-burst flush ACK.
         *
         * The in-loop check above only fires when burstCount reaches
         * BU_FW_BURST_SIZE (16). A trailing partial burst (e.g. the
         * last 7 frames of a 487-frame image) would never trigger
         * it — the sender would stream those frames and then sit
         * waiting for an ACK that never comes.
         *
         * Once we've drained at least imageSize bytes from the ring,
         * the image is fully written and we owe the sender an ACK
         * for the partial burst regardless of its size. Mirrors the
         * same fix in fw_bu_image_rx.c on the S-Board staging side. */
        if (fwInfo.imageSize > 0U &&
            fwBytesReceived >= fwInfo.imageSize &&
            fwBurstCount > 0U)
        {
            buFw_sendResponse(BU_RESP_FW_ACK, fwLastSeq);
            fwBurstCount = 0U;
        }
    }

    return fwState;
}

/* ═══════════════════════════════════════════════════════════════
 *  COMMAND HANDLERS
 * ═══════════════════════════════════════════════════════════════ */
static void buFw_handlePrepare(void)
{
    fwState         = BU_FW_PREPARING;
    fwBytesReceived = 0U;
    fwFramesReceived = 0U;
    fwBurstCount    = 0U;
    fwRingHead      = 0U;
    fwRingTail      = 0U;
    fwWriteAddr     = BU_BANK2_START;
    memset(&fwInfo, 0, sizeof(fwInfo));

    if (!buFw_eraseStagingBank())
    {
        buFw_sendResponse(BU_RESP_FW_NAK, 0U);
        fwState = BU_FW_IDLE;
        return;
    }

    fwState = BU_FW_WAITING_HEADER;
    buFw_sendResponse(BU_RESP_FW_ACK, 0U);
}

static void buFw_handleHeader(void)
{
    if (fwState != BU_FW_WAITING_HEADER)
    {
        buFw_sendResponse(BU_RESP_FW_NAK, 0U);
        return;
    }

    /* Header layout (matches S-Board fw_bu_master.c FW_BuM_sendHeader):
     *   data[4..7]   imageSize
     *   data[8..11]  imageCRC
     *   data[12..15] version
     */
    const uint8_t *p = fwPendingMsg.data;
    fwInfo.imageSize = (uint32_t)p[4]  | ((uint32_t)p[5]  << 8) |
                       ((uint32_t)p[6]  << 16) | ((uint32_t)p[7]  << 24);
    fwInfo.imageCRC  = (uint32_t)p[8]  | ((uint32_t)p[9]  << 8) |
                       ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
    fwInfo.version   = (uint32_t)p[12] | ((uint32_t)p[13] << 8) |
                       ((uint32_t)p[14] << 16) | ((uint32_t)p[15] << 24);

    fwState = BU_FW_RECEIVING;
    buFw_sendResponse(BU_RESP_FW_ACK, 0U);
}

static void buFw_handleVerify(void)
{
    fwState = BU_FW_VERIFYING;

    /* Drain any final partial burst before computing CRC. */
    if (fwBurstCount > 0U)
    {
        buFw_sendResponse(BU_RESP_FW_ACK, fwLastSeq);
        fwBurstCount = 0U;
    }

    uint32_t computed = buFw_calculateCRC32(BU_BANK2_START, fwInfo.imageSize);

    if (computed == fwInfo.imageCRC)
    {
        buFw_sendResponse(BU_RESP_FW_VERIFY_PASS, 0U);
    }
    else
    {
        buFw_sendResponse(BU_RESP_FW_VERIFY_FAIL, 0U);
        fwState = BU_FW_IDLE;
    }
}

static void buFw_handleActivate(void)
{
    fwState = BU_FW_ACTIVATING;

    /* Acknowledge ACTIVATE BEFORE we yank the chip into reset.
     * The S-Board's master state machine waits up to FW_BU_TMO_BOOT_MS
     * (10 s) for this BOOT_OK; if it never arrives it reports
     * FW_BU_M_ERR_TIMEOUT to the M-Board, which then reports the
     * whole OTA as failed even though the install actually
     * succeeded.  Send it first, busy-wait for TX completion (the
     * busy-wait is inside buFw_sendResponse), then trigger the
     * flag write + reset. */
    buFw_sendResponse(BU_RESP_FW_BOOT_OK, 0U);

    /* Write boot flag and reset. Function never returns on success. */
    buFw_writeBootFlag();

    /* If we get here, the flag write failed -- fall back to the
     * existing firmware.  The S-Board's master already received
     * BOOT_OK above, so it will mark the OTA DONE and then wait
     * for our post-boot RESP_FW_RESULT.  Since we never reset,
     * that announce never arrives and the M-Board will warn
     * (not fail) on the missing announce. */
    fwState = BU_FW_IDLE;
}

static void buFw_handleAbort(void)
{
    fwState         = BU_FW_IDLE;
    fwPendingCmd    = 0U;
    fwRingHead      = 0U;
    fwRingTail      = 0U;
    fwBytesReceived = 0U;
    fwFramesReceived = 0U;
    fwBurstCount    = 0U;
    memset(&fwInfo, 0, sizeof(fwInfo));
    buFw_sendResponse(BU_RESP_FW_ACK, 0U);
}

/* ═══════════════════════════════════════════════════════════════
 *  FLASH OPS — REAL or STUB
 * ═══════════════════════════════════════════════════════════════ */
#if BU_FW_FLASH_STUB

static bool buFw_eraseStagingBank(void)
{
    /* No flash available in stub build. Pretend success so the CAN
     * protocol can still be exercised end-to-end. */
    return true;
}

static void buFw_writePayloadToFlash(const uint8_t *payload)
{
    /* Stub: just advance the write address as if data were written. */
    fwWriteAddr += BU_FW_PAYLOAD_BYTES / 2U;   /* 60 bytes = 30 words */
    (void)payload;
}

static void buFw_writeBootFlag(void)
{
    /* Stub: cannot truly reboot into a new image without flash.
     * Just reset so the master's BOOT_OK timer expires and reports
     * failure (which is the honest outcome with no FAPI lib linked). */
    SysCtl_resetDevice();
}

#else  /* real flash path — requires FAPI lib */

#pragma CODE_SECTION(buFw_eraseStagingBank, ".TI.ramfunc")
static bool buFw_eraseStagingBank(void)
{
    Fapi_StatusType st;
    uint32_t i;

    EALLOW;
    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    for (i = 0; i < BU_BANK2_SECTORS; i++)
    {
        uint32_t addr = BU_BANK2_START + (i * 0x400U);

        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

        st = Fapi_issueAsyncCommandWithAddress(
            Fapi_EraseSector, (uint32_t *)addr);
        while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}
        if (st != Fapi_Status_Success)
        {
            EDIS;
            return false;
        }
        if (Fapi_getFsmStatus() != 3U)
        {
            EDIS;
            return false;
        }
    }
    EDIS;
    return true;
}

#pragma CODE_SECTION(buFw_writePayloadToFlash, ".TI.ramfunc")
static void buFw_writePayloadToFlash(const uint8_t *payload)
{
    Fapi_StatusType st;
    uint16_t wordBuf[8];
    uint16_t chunk, w;

    /* 64 bytes = 32 words = 4 × 8-word (512-bit) chunks. Each
     * chunk maps to one Fapi_issueProgrammingCommand call. The
     * payload is delivered straight from the CAN frame's data[0..63]
     * with no header bytes, so flash gets bin[N..N+63] CONTIGUOUSLY
     * — no intra-frame padding. Matches the S-Board self-update
     * format exactly, so the host-side CRC over the .bin file
     * (after pad_to_64) matches the BU's CRC over Bank 2. */
    for (chunk = 0; chunk < 4U; chunk++)
    {
        const uint8_t *src = payload + (chunk * 16U);
        for (w = 0; w < 8U; w++)
        {
            wordBuf[w] = (uint16_t)src[w * 2U] |
                         ((uint16_t)src[w * 2U + 1U] << 8);
        }

        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

        st = Fapi_issueProgrammingCommand(
            (uint32_t *)fwWriteAddr, wordBuf, 8U, 0, 0,
            Fapi_AutoEccGeneration);

        while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy) {}

        if (st != Fapi_Status_Success || Fapi_getFsmStatus() != 3U)
            return;

        fwWriteAddr += 8U;   /* advance by 8 words (16 bytes) */
    }
}

/* Debug CAN ID for "write-flag result" diagnostics. Broadcast after
 * EDIS (flash unlocked), before SysCtl_resetDevice, so the host can
 * see exactly what the flag write did. */
#define BU_FW_WRITE_FLAG_DEBUG_CAN_ID   0x2AU

#pragma CODE_SECTION(buFw_writeBootFlag, ".TI.ramfunc")
static void buFw_writeBootFlag(void)
{
    uint16_t flag[8];
    Fapi_StatusType      eraseSt     = Fapi_Status_Success;
    Fapi_StatusType      programSt   = Fapi_Status_Success;
    Fapi_FlashStatusType eraseFsm    = 0;
    Fapi_FlashStatusType programFsm  = 0;

    EALLOW;
    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);

    /* Use the same pattern as the (verified-working) S-Board self-
     * update path: active bank = Bank 0 for all operations including
     * the Bank 3 flag write. If this silently fails on the BU but
     * works on the S-Board, the diagnostic frame broadcast below
     * will reveal exactly which FAPI call returned what. */
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    /* ── Erase Bank 3 sector 0 ─────────────────────────────── */
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

    eraseSt = Fapi_issueAsyncCommandWithAddress(
        Fapi_EraseSector, (uint32_t *)BU_BANK3_FLAG_ADDR);
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}
    eraseFsm = Fapi_getFsmStatus();

    /* Build flag */
    flag[0] = BU_BOOT_FLAG_PENDING;            /* 0xA5A5 */
    flag[1] = BU_BOOT_FLAG_CRC_VALID;          /* 0x5A5A */
    flag[2] = (uint16_t)(fwInfo.imageSize        & 0xFFFFU);
    flag[3] = (uint16_t)((fwInfo.imageSize >> 16) & 0xFFFFU);
    flag[4] = (uint16_t)(fwInfo.imageCRC         & 0xFFFFU);
    flag[5] = (uint16_t)((fwInfo.imageCRC  >> 16) & 0xFFFFU);
    flag[6] = 0xFFFFU;
    flag[7] = 0xFFFFU;

    /* ── Program 8 words at Bank 3 sector 0 ─────────────────── */
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

    programSt = Fapi_issueProgrammingCommand(
        (uint32_t *)BU_BANK3_FLAG_ADDR, flag, 8U, 0, 0,
        Fapi_AutoEccGeneration);
    while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy) {}
    programFsm = Fapi_getFsmStatus();

    EDIS;

    /* ── Read back the flag from flash ──────────────────────── */
    {
        volatile uint16_t *fr = (volatile uint16_t *)BU_BANK3_FLAG_ADDR;
        uint16_t rb0 = fr[0];
        uint16_t rb1 = fr[1];

        /* ── Broadcast diagnostic frame on CAN ID 0x2A ─────────
         * Layout:
         *   [0] = 0xBF tag byte
         *   [1] = erase Fapi return code
         *   [2] = erase FSM status after wait
         *   [3] = program Fapi return code
         *   [4] = program FSM status after wait
         *   [5] = readback flag[0] low byte (should be 0xA5)
         *   [6] = readback flag[0] high byte (should be 0xA5)
         *   [7] = readback flag[1] low byte (should be 0x5A)
         *
         * If all four status bytes are 0/0/0/0 (success/success) and
         * rb0==0xA5A5/rb1==0x5A5A, the flag was written correctly. */
        MCAN_TxBufElement dbg;
        memset(&dbg, 0, sizeof(dbg));
        dbg.id  = ((uint32_t)BU_FW_WRITE_FLAG_DEBUG_CAN_ID) << 18U;
        dbg.brs = 0x1;
        dbg.dlc = 15;
        dbg.fdf = 0x1;
        dbg.efc = 1U;
        dbg.mm  = 0xBFU;
        dbg.data[0] = 0xBFU;
        dbg.data[1] = (uint8_t)eraseSt;
        dbg.data[2] = (uint8_t)eraseFsm;
        dbg.data[3] = (uint8_t)programSt;
        dbg.data[4] = (uint8_t)programFsm;
        dbg.data[5] = (uint8_t)(rb0 & 0xFFU);
        dbg.data[6] = (uint8_t)((rb0 >> 8) & 0xFFU);
        dbg.data[7] = (uint8_t)(rb1 & 0xFFU);

        MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 4U, &dbg);
        MCAN_txBufAddReq(CAN_BU_BASE, 4U);
        /* Short delay so the frame actually leaves the bus before
         * we reset. Roughly a handful of CAN-FD frame times. */
        {
            volatile uint32_t d;
            for (d = 0; d < 500000U; d++) { }
        }
    }

    SysCtl_resetDevice();
}

#endif /* BU_FW_FLASH_STUB */

/* ═══════════════════════════════════════════════════════════════
 *  CRC32 — over flash region (host computes the same value over
 *          the raw .bin file, so this must match byte-for-byte).
 * ═══════════════════════════════════════════════════════════════ */
static uint32_t buFw_calculateCRC32(uint32_t startAddr, uint32_t numBytes)
{
    uint32_t crc = 0xFFFFFFFFU;
    volatile uint16_t *wordPtr = (volatile uint16_t *)startAddr;
    uint32_t numWords = (numBytes + 1U) / 2U;
    uint32_t i;

    for (i = 0; i < numWords; i++)
    {
        uint16_t word = wordPtr[i];
        crc = (crc >> 8) ^ fwCrc32Table[(crc ^ (word & 0xFFU)) & 0xFFU];
        crc = (crc >> 8) ^ fwCrc32Table[(crc ^ (word >> 8)) & 0xFFU];
    }
    return crc ^ 0xFFFFFFFFU;
}

/* ═══════════════════════════════════════════════════════════════
 *  HELPERS
 * ═══════════════════════════════════════════════════════════════ */
static bool buFw_isAddressedToMe(const MCAN_RxBufElement *rxElem)
{
    uint8_t target = rxElem->data[1];
    return (target == fwMyBoardId) || (target == BU_FW_BROADCAST_ID);
}

static void buFw_sendResponse(uint8_t respCode, uint16_t seq)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)BU_FW_RESP_CAN_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xCFU;

    TxMsg.data[0] = respCode;
    TxMsg.data[1] = fwMyBoardId;
    TxMsg.data[2] = (uint8_t)(seq & 0xFFU);
    TxMsg.data[3] = (uint8_t)((seq >> 8) & 0xFFU);
    TxMsg.data[4] = (uint8_t)fwState;
    TxMsg.data[5] = (uint8_t)(fwFramesReceived & 0xFFU);
    TxMsg.data[6] = (uint8_t)((fwFramesReceived >> 8) & 0xFFU);
    TxMsg.data[7] = (uint8_t)((fwFramesReceived >> 16) & 0xFFU);

    /* Use TX buffer 4 — buffers 0..3 already committed by ops. */
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 4U, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 4U);
    while ((MCAN_getTxBufReqPend(CAN_BU_BASE) & (1UL << 4U)) != 0U)
        ;
}
