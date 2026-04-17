/*
 * fw_image_rx.c
 *
 *  Created on: 03-Apr-2026
 *      Author: Parthasarathy.M
 *
 *  Receives firmware binary from M-Board over CAN-FD, writes to Bank 2.
 *
 *  CAN IDs:  6 = data (64 bytes/frame),  7 = command,  8 = response
 *
 *  Flow:
 *    CMD_FW_START  → erase Bank 2 → ACK
 *    CMD_FW_HEADER → parse image info → ACK
 *    Data frames (ID 6) → ring buffer (ISR) → flash (main loop) → ACK every 16
 *    CMD_FW_COMPLETE → verify CRC → CRC_PASS / CRC_FAIL
 */

#include "fw_image_rx.h"
#include "can_driver.h"
#include <string.h>

/* ── State ────────────────────────────────────────────────────── */
static volatile FW_RxState  fwState = FW_IDLE;

/* ── Image info ───────────────────────────────────────────────── */
static FW_ImageInfo         fwImageInfo;

/* ── Ring buffer (ISR → main loop) ────────────────────────────── */
static FW_RxFrame           fwRingBuf[FW_RX_RING_SIZE];
static volatile uint16_t    fwRingHead = 0;    /* ISR writes here */
static uint16_t             fwRingTail = 0;    /* main loop reads here */

/* ── Write tracking ───────────────────────────────────────────── */
static uint32_t             fwWriteAddr;        /* next flash write address */
static uint32_t             fwFramesReceived;   /* total data frames received */
static uint16_t             fwBurstCount;       /* frames in current burst */

/* ── Pending command (ISR sets, main loop processes) ──────────── */
static volatile uint8_t     fwPendingCmd = 0;
static MCAN_RxBufElement    fwCmdMsg;           /* copy of command frame */

/* ── Forward declarations ─────────────────────────────────────── */
static void FW_sendResponse(uint8_t respCode, uint16_t seq);
static void FW_sendResponseWithData(uint8_t respCode, const uint8_t *data, uint16_t len);
static void FW_handleStart(void);
static void FW_handleHeader(void);
static void FW_handleComplete(void);
static void FW_handleReadFlag(void);
static void FW_handleClearFlag(void);
static void FW_handleResetDevice(void);
static void FW_handleReadFlash(void);
static void FW_handleComputeCRC(void);
static void FW_handleEraseSector(void);
static void FW_handleWriteFlag(void);
static void FW_handleGetState(void);
static void FW_writeFrameToFlash(const uint8_t *frameData);
static uint32_t FW_calculateCRC32(uint32_t startAddr, uint32_t numBytes);
static void FW_writeBootFlag(void);

/* ═══════════════════════════════════════════════════════════════
 *  INIT — Add MCANA filters for ID 6 (data) and ID 7 (command)
 * ═══════════════════════════════════════════════════════════════ */
void FW_ImageRx_init(void)
{
    /* Filters for ID 6 (data) and ID 7 (command) are configured in
     * MCANAConfig() (can_driver.c) — must be added before MCAN leaves
     * SW_INIT mode. This function just resets state. */
    fwState = FW_IDLE;
    fwRingHead = 0;
    fwRingTail = 0;
    fwPendingCmd = 0;
}

/* ═══════════════════════════════════════════════════════════════
 *  ISR CALLBACKS — Called from MCANIntr1ISR, must be fast
 * ═══════════════════════════════════════════════════════════════ */

/* Data frame (ID 6) arrived — copy 64 bytes into ring buffer */
void FW_ImageRx_isrOnData(const MCAN_RxBufElement *rxElem)
{
    if (fwState != FW_RECEIVING)
        return;

    uint16_t nextHead = (fwRingHead + 1) & FW_RX_RING_MASK;
    if (nextHead == fwRingTail)
        return;  /* ring full — drop frame, sender will retry */

    memcpy(fwRingBuf[fwRingHead].data, rxElem->data, FW_FRAME_SIZE);
    fwRingHead = nextHead;
}

/* Command frame (ID 7) arrived — save it for main loop */
void FW_ImageRx_isrOnCommand(const MCAN_RxBufElement *rxElem)
{
    memcpy(&fwCmdMsg, rxElem, sizeof(MCAN_RxBufElement));
    fwPendingCmd = rxElem->data[0];
}

/* ═══════════════════════════════════════════════════════════════
 *  MAIN LOOP — Call this from the main while(1) loop
 * ═══════════════════════════════════════════════════════════════ */
FW_RxState FW_ImageRx_process(void)
{
    /* 1. Handle any pending command */
    uint8_t cmd = fwPendingCmd;
    if (cmd != 0)
    {
        fwPendingCmd = 0;

        switch (cmd)
        {
            case CMD_FW_START:
                FW_handleStart();
                break;

            case CMD_FW_HEADER:
                FW_handleHeader();
                break;

            case CMD_FW_COMPLETE:
                FW_handleComplete();
                break;

            case CMD_READ_FLAG:
                FW_handleReadFlag();
                break;

            case CMD_CLEAR_FLAG:
                FW_handleClearFlag();
                break;

            case CMD_RESET_DEVICE:
                FW_handleResetDevice();
                break;

            case CMD_READ_FLASH:
                FW_handleReadFlash();
                break;

            case CMD_COMPUTE_CRC:
                FW_handleComputeCRC();
                break;

            case CMD_ERASE_SECTOR:
                FW_handleEraseSector();
                break;

            case CMD_WRITE_FLAG:
                FW_handleWriteFlag();
                break;

            case CMD_GET_STATE:
                FW_handleGetState();
                break;

            default:
                break;
        }
    }

    /* 2. Drain ring buffer → flash (only while receiving) */
    if (fwState == FW_RECEIVING)
    {
        while (fwRingTail != fwRingHead)
        {
            EALLOW;
            FW_writeFrameToFlash(fwRingBuf[fwRingTail].data);
            EDIS;
            fwRingTail = (fwRingTail + 1) & FW_RX_RING_MASK;

            fwFramesReceived++;
            fwBurstCount++;

            /* ACK every BURST_SIZE frames */
            if (fwBurstCount >= FW_BURST_SIZE)
            {
                /* 0x14: Data progress — frames received, write address */
                FW_sendDebug(0x14, (uint8_t)fwFramesReceived,
                             (uint8_t)(fwFramesReceived >> 8),
                             fwWriteAddr, fwImageInfo.totalFrames);

                FW_sendResponse(RESP_FW_ACK, (uint16_t)(fwFramesReceived - 1));
                fwBurstCount = 0;
            }
        }

        /* ACK final partial burst if we've received all expected frames */
        if (fwFramesReceived >= fwImageInfo.totalFrames && fwBurstCount > 0)
        {
            FW_sendResponse(RESP_FW_ACK, (uint16_t)(fwFramesReceived - 1));
            fwBurstCount = 0;
        }
    }

    return fwState;
}

/* ═══════════════════════════════════════════════════════════════
 *  DEBUG CAN — Send a short debug frame on a unique CAN ID
 *
 *  Debug CAN IDs:
 *    0x10 = CMD_FW_START received
 *    0x11 = Bank 2 erase started
 *    0x12 = Bank 2 erase complete (or failed)
 *    0x13 = CMD_FW_HEADER received & parsed
 *    0x14 = Data frame written to flash (every 16th frame)
 *    0x15 = CMD_FW_COMPLETE received
 *    0x16 = CRC computation done
 *    0x17 = Boot flag written, about to reset
 *    0x18 = Erase/write error detail
 * ═══════════════════════════════════════════════════════════════ */
static void FW_sendDebug(uint16_t canId, uint8_t b0, uint8_t b1,
                          uint32_t val1, uint32_t val2)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)canId) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 8;       /* 8 bytes (classic CAN size, keeps it small) */
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xDDU;

    TxMsg.data[0] = b0;
    TxMsg.data[1] = b1;
    TxMsg.data[2] = (val1 >>  0) & 0xFF;
    TxMsg.data[3] = (val1 >>  8) & 0xFF;
    TxMsg.data[4] = (val1 >> 16) & 0xFF;
    TxMsg.data[5] = (val1 >> 24) & 0xFF;
    TxMsg.data[6] = (val2 >>  0) & 0xFF;
    TxMsg.data[7] = (val2 >>  8) & 0xFF;

    /* S-Board self-update responses go to M-Board via MCANB */
    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 2U, &TxMsg);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 2U);
    canTxWaitComplete(CAN_MBOARD_BASE);
}

/* ═══════════════════════════════════════════════════════════════
 *  COMMAND HANDLERS
 * ═══════════════════════════════════════════════════════════════ */

/* CMD_FW_START: Erase Bank 2, prepare for receiving */
volatile uint32_t fwDbgEraseError = 0;
volatile uint32_t fwDbgEraseSector = 0;
volatile uint32_t fwDbgFsmStatus = 0;

static void FW_handleStart(void)
{
    fwState = FW_ERASING;
    updateFailed = false;
    fwDbgEraseError = 0;
    fwDbgEraseSector = 0;
    fwDbgFsmStatus = 0;

    /* 0x10: CMD_FW_START received */
    FW_sendDebug(0x10, 0x30, (uint8_t)fwState, 0, 0);

    /* Re-init Flash API */
    EALLOW;
    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    /* 0x11: Erase starting */
    FW_sendDebug(0x11, FW_BANK2_SECTORS, 0, FW_BANK2_START, 0);

    /* Erase all 128 sectors of Bank 2 */
    FW_eraseFlashRange(FW_BANK2_START, FW_BANK2_SECTORS);
    EDIS;

    if (updateFailed)
    {
        /* 0x18: Erase error — send error code, sector, FSM status */
        FW_sendDebug(0x18, (uint8_t)fwDbgEraseError, (uint8_t)fwDbgEraseSector,
                     fwDbgFsmStatus, fwDbgEraseError);

        /* 0x12: Erase FAILED */
        FW_sendDebug(0x12, 0xFF, 0x00, fwDbgEraseSector, fwDbgFsmStatus);

        FW_sendResponse(RESP_FW_NAK, 0);
        fwState = FW_IDLE;
        return;
    }

    /* 0x12: Erase complete OK */
    FW_sendDebug(0x12, 0x01, 0x00, FW_BANK2_SECTORS, 0);

    /* Reset counters */
    fwWriteAddr      = FW_BANK2_START;
    fwFramesReceived = 0;
    fwBurstCount     = 0;
    fwRingHead       = 0;
    fwRingTail       = 0;
    memset(&fwImageInfo, 0, sizeof(FW_ImageInfo));

    fwState = FW_WAITING_HEADER;
    FW_sendResponse(RESP_FW_ACK, 0);
}

/* CMD_FW_HEADER: Parse image metadata */
static void FW_handleHeader(void)
{
    if (fwState != FW_WAITING_HEADER)
    {
        FW_sendDebug(0x13, 0xFF, (uint8_t)fwState, 0, 0);
        FW_sendResponse(RESP_FW_NAK, 0);
        return;
    }

    /* Parse header from command frame payload (bytes 4-63) */
    const uint8_t *p = &fwCmdMsg.data[4];

    fwImageInfo.imageSize   = p[4]  | (p[5] << 8)  | ((uint32_t)p[6] << 16)  | ((uint32_t)p[7] << 24);
    fwImageInfo.imageCRC    = p[8]  | (p[9] << 8)  | ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
    fwImageInfo.version     = p[12] | (p[13] << 8) | ((uint32_t)p[14] << 16) | ((uint32_t)p[15] << 24);
    fwImageInfo.totalFrames = p[24] | (p[25] << 8) | ((uint32_t)p[26] << 16) | ((uint32_t)p[27] << 24);

    /* 0x13: Header parsed — size, totalFrames */
    FW_sendDebug(0x13, 0x01, (uint8_t)fwImageInfo.version,
                 fwImageInfo.imageSize, fwImageInfo.totalFrames);

    fwState = FW_RECEIVING;
    FW_sendResponse(RESP_FW_ACK, 0);
}

/* CMD_FW_COMPLETE: Verify CRC over written flash data */
static void FW_handleComplete(void)
{
    /* 0x15: CMD_FW_COMPLETE received */
    FW_sendDebug(0x15, (uint8_t)fwFramesReceived, (uint8_t)(fwFramesReceived >> 8),
                 fwImageInfo.imageSize, fwImageInfo.imageCRC);

    fwState = FW_VERIFYING;

    uint32_t computedCRC = FW_calculateCRC32(FW_BANK2_START, fwImageInfo.imageSize);

    /* 0x16: CRC done — expected vs computed */
    FW_sendDebug(0x16, (computedCRC == fwImageInfo.imageCRC) ? 0x01 : 0x00, 0x00,
                 fwImageInfo.imageCRC, computedCRC);

    if (computedCRC == fwImageInfo.imageCRC)
    {
        FW_sendResponse(RESP_FW_CRC_PASS, 0);

        /* 0x17: Writing boot flag, about to reset */
        FW_sendDebug(0x17, 0x01, 0x00, fwImageInfo.imageSize, fwImageInfo.imageCRC);

        FW_triggerUpdate();
        /* Never reaches here */
    }
    else
    {
        FW_sendResponse(RESP_FW_CRC_FAIL, 0);
    }

    fwState = FW_IDLE;
}

/* ═══════════════════════════════════════════════════════════════
 *  FLASH WRITE — Write one 64-byte (32-word) frame to flash
 *
 *  64 bytes = 32 uint16 words = 4 × 8-word (512-bit) writes
 * ═══════════════════════════════════════════════════════════════ */
#pragma CODE_SECTION(FW_writeFrameToFlash, ".TI.ramfunc");
static void FW_writeFrameToFlash(const uint8_t *frameData)
{
    Fapi_StatusType  oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    uint16_t wordBuf[8];
    uint16_t chunk, w;

    /* 4 chunks of 8 words = 32 words = 64 bytes */
    for (chunk = 0; chunk < 4; chunk++)
    {
        /* Convert 16 bytes → 8 uint16 words (little-endian) */
        const uint8_t *src = frameData + (chunk * 16);
        for (w = 0; w < 8; w++)
        {
            wordBuf[w] = src[w * 2] | ((uint16_t)src[w * 2 + 1] << 8);
        }

        ClearFSMStatus();

        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

        oReturnCheck = Fapi_issueProgrammingCommand(
            (uint32_t *)fwWriteAddr, wordBuf, 8, 0, 0,
            Fapi_AutoEccGeneration);

        while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy)
            ;

        if (oReturnCheck != Fapi_Status_Success)
        {
            Example_Error(oReturnCheck);
            return;
        }

        oFlashStatus = Fapi_getFsmStatus();
        if (oFlashStatus != 3)
        {
            FMSTAT_Fail();
            return;
        }

        fwWriteAddr += 8;  /* advance by 8 words */
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  CRC32 — Lookup-table based (fast, saves CPU cycles)
 * ═══════════════════════════════════════════════════════════════ */
static const uint32_t crc32Table[256] = {
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

static uint32_t FW_calculateCRC32(uint32_t startAddr, uint32_t numBytes)
{
    uint32_t crc = 0xFFFFFFFF;
    volatile uint16_t *wordPtr = (volatile uint16_t *)startAddr;
    uint32_t numWords = (numBytes + 1) / 2;  /* bytes → 16-bit words */
    uint32_t i;

    for (i = 0; i < numWords; i++)
    {
        uint16_t word = wordPtr[i];
        /* Process low byte */
        crc = (crc >> 8) ^ crc32Table[(crc ^ (word & 0xFF)) & 0xFF];
        /* Process high byte */
        crc = (crc >> 8) ^ crc32Table[(crc ^ (word >> 8)) & 0xFF];
    }

    return crc ^ 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════
 *  EXTENDED COMMAND HANDLERS
 * ═══════════════════════════════════════════════════════════════ */

/* CMD_READ_FLAG: Read boot flag from Bank 3 and send back */
static void FW_handleReadFlag(void)
{
    volatile uint16_t *flag = (volatile uint16_t *)FW_BANK3_FLAG_ADDR;
    uint8_t resp[16];

    resp[0]  = (uint8_t)(flag[0] & 0xFF);        /* updatePending low */
    resp[1]  = (uint8_t)(flag[0] >> 8);           /* updatePending high */
    resp[2]  = (uint8_t)(flag[1] & 0xFF);        /* crcValid low */
    resp[3]  = (uint8_t)(flag[1] >> 8);           /* crcValid high */
    /* imageSize */
    resp[4]  = (uint8_t)(flag[2] & 0xFF);
    resp[5]  = (uint8_t)(flag[2] >> 8);
    resp[6]  = (uint8_t)(flag[3] & 0xFF);
    resp[7]  = (uint8_t)(flag[3] >> 8);
    /* imageCRC */
    resp[8]  = (uint8_t)(flag[4] & 0xFF);
    resp[9]  = (uint8_t)(flag[4] >> 8);
    resp[10] = (uint8_t)(flag[5] & 0xFF);
    resp[11] = (uint8_t)(flag[5] >> 8);
    /* padding words */
    resp[12] = (uint8_t)(flag[6] & 0xFF);
    resp[13] = (uint8_t)(flag[6] >> 8);
    resp[14] = (uint8_t)(flag[7] & 0xFF);
    resp[15] = (uint8_t)(flag[7] >> 8);

    FW_sendResponseWithData(RESP_FLAG_DATA, resp, 16);
}

/* CMD_CLEAR_FLAG: Erase Bank 3 sector 0 */
#pragma CODE_SECTION(FW_handleClearFlag, ".TI.ramfunc")
static void FW_handleClearFlag(void)
{
    EALLOW;
    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    ClearFSMStatus();
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);
    Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector,
                                      (uint32_t *)FW_BANK3_FLAG_ADDR);
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}
    EDIS;

    FW_sendResponse(RESP_FW_ACK, 0);
}

/* CMD_RESET_DEVICE: Send ACK then reset */
static void FW_handleResetDevice(void)
{
    FW_sendResponse(RESP_FW_ACK, 0);
    /* Small delay to let the ACK frame complete TX */
    volatile uint32_t d;
    for (d = 0; d < 500000U; d++) {}
    SysCtl_resetDevice();
}

/* CMD_READ_FLASH: Read 8 words from address in payload[4..7] */
static void FW_handleReadFlash(void)
{
    const uint8_t *p = &fwCmdMsg.data[4];
    uint32_t addr = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    volatile uint16_t *wordPtr = (volatile uint16_t *)addr;
    uint8_t resp[16];
    uint16_t i;

    for (i = 0; i < 8; i++)
    {
        resp[i * 2]     = (uint8_t)(wordPtr[i] & 0xFF);
        resp[i * 2 + 1] = (uint8_t)(wordPtr[i] >> 8);
    }

    FW_sendResponseWithData(RESP_FLASH_DATA, resp, 16);
}

/* CMD_COMPUTE_CRC: CRC32 over address range
 *   payload[4..7] = startAddr
 *   payload[8..11] = numBytes */
static void FW_handleComputeCRC(void)
{
    const uint8_t *p = &fwCmdMsg.data[4];
    uint32_t addr = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    uint32_t size = (uint32_t)p[4] | ((uint32_t)p[5] << 8) |
                    ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);

    uint32_t crc = FW_calculateCRC32(addr, size);

    uint8_t resp[8];
    resp[0] = (uint8_t)(crc);       resp[1] = (uint8_t)(crc >> 8);
    resp[2] = (uint8_t)(crc >> 16); resp[3] = (uint8_t)(crc >> 24);
    resp[4] = (uint8_t)(addr);      resp[5] = (uint8_t)(addr >> 8);
    resp[6] = (uint8_t)(size);      resp[7] = (uint8_t)(size >> 8);

    FW_sendResponseWithData(RESP_CRC_DATA, resp, 8);
}

/* CMD_ERASE_SECTOR: Erase one sector at address in payload[4..7] */
#pragma CODE_SECTION(FW_handleEraseSector, ".TI.ramfunc")
static void FW_handleEraseSector(void)
{
    const uint8_t *p = &fwCmdMsg.data[4];
    uint32_t addr = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                    ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    EALLOW;
    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    ClearFSMStatus();
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);
    Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, (uint32_t *)addr);
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}
    EDIS;

    FW_sendResponse(RESP_FW_ACK, 0);
}

/* CMD_WRITE_FLAG: Manually write boot flag with values from payload */
#pragma CODE_SECTION(FW_handleWriteFlag, ".TI.ramfunc")
static void FW_handleWriteFlag(void)
{
    const uint8_t *p = &fwCmdMsg.data[4];
    /* Parse: imageSize [4..7], imageCRC [8..11] */
    uint32_t imgSize = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    uint32_t imgCRC  = (uint32_t)p[4] | ((uint32_t)p[5] << 8) |
                       ((uint32_t)p[6] << 16) | ((uint32_t)p[7] << 24);

    uint16_t flagData[8];
    flagData[0] = FW_FLAG_UPDATE_PENDING;
    flagData[1] = FW_FLAG_CRC_VALID;
    flagData[2] = (uint16_t)(imgSize & 0xFFFF);
    flagData[3] = (uint16_t)(imgSize >> 16);
    flagData[4] = (uint16_t)(imgCRC & 0xFFFF);
    flagData[5] = (uint16_t)(imgCRC >> 16);
    flagData[6] = 0xFFFF;
    flagData[7] = 0xFFFF;

    EALLOW;
    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    /* Erase Bank 3 sector 0 first */
    ClearFSMStatus();
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);
    Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector,
                                      (uint32_t *)FW_BANK3_FLAG_ADDR);
    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}

    /* Write flag data */
    ClearFSMStatus();
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);
    Fapi_issueProgrammingCommand((uint32_t *)FW_BANK3_FLAG_ADDR, flagData, 8,
                                 0, 0, Fapi_AutoEccGeneration);
    while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy) {}
    EDIS;

    FW_sendResponse(RESP_FW_ACK, 0);
}

/* CMD_GET_STATE: Report current state machine state */
static void FW_handleGetState(void)
{
    uint8_t resp[8];
    resp[0] = (uint8_t)fwState;
    resp[1] = (uint8_t)(fwFramesReceived & 0xFF);
    resp[2] = (uint8_t)((fwFramesReceived >> 8) & 0xFF);
    resp[3] = (uint8_t)fwBurstCount;
    resp[4] = (uint8_t)(fwWriteAddr & 0xFF);
    resp[5] = (uint8_t)((fwWriteAddr >> 8) & 0xFF);
    resp[6] = (uint8_t)((fwWriteAddr >> 16) & 0xFF);
    resp[7] = (uint8_t)((fwWriteAddr >> 24) & 0xFF);

    FW_sendResponseWithData(RESP_STATE_DATA, resp, 8);
}

/* ═══════════════════════════════════════════════════════════════
 *  SEND RESPONSE — TX on CAN ID 8 via MCANA
 * ═══════════════════════════════════════════════════════════════ */
/* ═══════════════════════════════════════════════════════════════
 *  BOOT FLAG — Write update flag to Bank 3, then reset MCU
 *
 *  Flag layout at 0x0E0000 (must match boot_manager.c):
 *    Word 0: 0xA5A5 (updatePending)
 *    Word 1: 0x5A5A (crcValid)
 *    Word 2: imageSize low 16 bits
 *    Word 3: imageSize high 16 bits
 *    Word 4: imageCRC low 16 bits
 *    Word 5: imageCRC high 16 bits
 *    Word 6-7: 0xFFFF (padding)
 * ═══════════════════════════════════════════════════════════════ */
/* FW_writeBootFlag — Fully self-contained, runs entirely from RAM.
 *
 * This function re-initializes the Flash API, sets EALLOW, erases
 * Bank 3 sector 0, programs the boot flag, and resets the device.
 * Everything happens inside RAM so there is zero flash access during
 * the erase/program operations.
 *
 * Previous bug: FW_triggerUpdate ran from flash and called this function.
 * When the erase started (locking ALL flash banks), the return address
 * pointed into flash — causing unpredictable stalls on return.
 * Fix: The entire erase → program → reset sequence now lives in RAM.
 */
#pragma CODE_SECTION(FW_writeBootFlag, ".TI.ramfunc")
static void FW_writeBootFlag(void)
{
    Fapi_StatusType oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    uint16_t flagData[8];

    /* Re-init Flash API inside the RAM function so we don't depend
     * on state from the earlier FW_handleStart call */
    EALLOW;

    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    /* ── Step 1: Erase Bank 3 sector 0 ──────────────────────── */
    ClearFSMStatus();

    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

    oReturnCheck = Fapi_issueAsyncCommandWithAddress(
        Fapi_EraseSector, (uint32_t *)FW_BANK3_FLAG_ADDR);

    while (Fapi_checkFsmForReady() != Fapi_Status_FsmReady) {}

    if (oReturnCheck != Fapi_Status_Success)
    {
        EDIS;
        return;
    }

    /* Verify erase succeeded (FSM status 3 = operation complete) */
    oFlashStatus = Fapi_getFsmStatus();
    if (oFlashStatus != 3)
    {
        EDIS;
        return;
    }

    /* ── Step 2: Build and program the flag data ────────────── */
    flagData[0] = FW_FLAG_UPDATE_PENDING;           /* 0xA5A5 */
    flagData[1] = FW_FLAG_CRC_VALID;                /* 0x5A5A */
    flagData[2] = (uint16_t)(fwImageInfo.imageSize & 0xFFFF);
    flagData[3] = (uint16_t)(fwImageInfo.imageSize >> 16);
    flagData[4] = (uint16_t)(fwImageInfo.imageCRC & 0xFFFF);
    flagData[5] = (uint16_t)(fwImageInfo.imageCRC >> 16);
    flagData[6] = 0xFFFF;
    flagData[7] = 0xFFFF;

    ClearFSMStatus();

    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
    Fapi_setupBankSectorEnable(
        FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

    oReturnCheck = Fapi_issueProgrammingCommand(
        (uint32_t *)FW_BANK3_FLAG_ADDR, flagData, 8,
        0, 0, Fapi_AutoEccGeneration);

    while (Fapi_checkFsmForReady() == Fapi_Status_FsmBusy) {}

    /* Verify program succeeded */
    oFlashStatus = Fapi_getFsmStatus();

    EDIS;

    /* ── Step 3: Reset — must happen AFTER EDIS and AFTER flash
     *    operations are fully complete. Do NOT return to flash code;
     *    reset directly from RAM. ────────────────────────────── */
    if (oReturnCheck == Fapi_Status_Success && oFlashStatus == 3)
    {
        SysCtl_resetDevice();
        /* unreachable */
    }
    /* If we get here, the flag write failed. Don't reset — let the
     * caller know something went wrong by returning normally.
     * The existing firmware will keep running. */
}

/* FW_triggerUpdate — Also runs from RAM so the entire path from
 * CRC_PASS → flag write → reset never touches flash. */
#pragma CODE_SECTION(FW_triggerUpdate, ".TI.ramfunc")
void FW_triggerUpdate(void)
{
    FW_writeBootFlag();

    /* If FW_writeBootFlag returned (instead of resetting), the flag
     * write failed. Fall through — the device stays running with the
     * old firmware. The GUI will notice no reset happened. */
}

/* ═══════════════════════════════════════════════════════════════
 *  SEND RESPONSE — TX on CAN ID 8 via MCANA
 * ═══════════════════════════════════════════════════════════════ */
static void FW_sendResponse(uint8_t respCode, uint16_t seq)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)FW_RESP_CAN_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;      /* 64 bytes */
    TxMsg.rtr = 0;
    TxMsg.xtd = 0;
    TxMsg.fdf = 0x1;
    TxMsg.esi = 0U;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xBBU;

    TxMsg.data[0] = respCode;
    TxMsg.data[1] = 0x01;              /* src = S-Board */
    TxMsg.data[2] = seq & 0xFF;        /* seq low byte */
    TxMsg.data[3] = (seq >> 8) & 0xFF; /* seq high byte */

    /* S-Board self-update responses go to M-Board via MCANB */
    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 1U, &TxMsg);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 1U);

    canTxWaitComplete(CAN_MBOARD_BASE);
}

/* Send response with arbitrary data payload (up to 60 bytes after 4-byte header) */
static void FW_sendResponseWithData(uint8_t respCode, const uint8_t *data, uint16_t len)
{
    MCAN_TxBufElement TxMsg;
    uint16_t i;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)FW_RESP_CAN_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xBBU;

    TxMsg.data[0] = respCode;
    TxMsg.data[1] = 0x01;
    TxMsg.data[2] = 0;
    TxMsg.data[3] = 0;

    for (i = 0; i < len && i < 60; i++)
        TxMsg.data[4 + i] = data[i];

    MCAN_writeMsgRam(CAN_MBOARD_BASE, MCAN_MEM_TYPE_BUF, 1U, &TxMsg);
    MCAN_txBufAddReq(CAN_MBOARD_BASE, 1U);
    canTxWaitComplete(CAN_MBOARD_BASE);
}
