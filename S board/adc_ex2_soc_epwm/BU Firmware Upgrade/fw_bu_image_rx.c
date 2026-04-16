/*
 * fw_bu_image_rx.c
 *
 *  Receives a BU board's firmware binary from the M-Board over CAN-FD
 *  and writes it into S-Board flash Bank 1 (0x0A0000).
 *
 *  Mirrors the structure of fw_image_rx.c (which targets Bank 2 for the
 *  S-Board's own self-update) but is fully independent so the two
 *  pipelines can run without interfering with each other.
 *
 *  After CMD_BU_FW_COMPLETE passes CRC verification, the state machine
 *  parks at FW_BU_RX_READY. fw_bu_master then takes over and streams
 *  the staged image to each BU board over MCANA.
 */

#include "fw_bu_image_rx.h"
#include "can_driver.h"
#include <string.h>

/* ── State ────────────────────────────────────────────────────── */
static volatile FW_BuRxState  fwBuState = FW_BU_RX_IDLE;
static FW_BuImageInfo         fwBuImageInfo;

/* ── Ring buffer (ISR -> main loop) ───────────────────────────── */
static FW_BuRxFrame           fwBuRingBuf[FW_BU_RX_RING_SIZE];
static volatile uint16_t      fwBuRingHead = 0;
static uint16_t               fwBuRingTail = 0;

/* ── Write tracking ───────────────────────────────────────────── */
static uint32_t               fwBuWriteAddr;
static uint32_t               fwBuFramesReceived;
static uint16_t               fwBuBurstCount;

/* ── Pending command (ISR sets, main loop processes) ──────────── */
static volatile uint8_t       fwBuPendingCmd = 0;
static MCAN_RxBufElement      fwBuCmdMsg;

/* ── CRC32 (lookup-table) — local copy avoids coupling to fw_image_rx.c ── */
static const uint32_t fwBuCrc32Table[256] = {
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
static void     FW_Bu_handleStart(void);
static void     FW_Bu_handleHeader(void);
static void     FW_Bu_handleComplete(void);
static void     FW_Bu_handleAbort(void);
static void     FW_Bu_handleGetState(void);
static void     FW_Bu_writeFrameToFlash(const uint8_t *frameData);
static uint32_t FW_Bu_calculateCRC32(uint32_t startAddr, uint32_t numBytes);
static void     FW_Bu_sendResponse(uint8_t respCode, uint16_t seq);
static void     FW_Bu_sendStateResponse(void);

/* ═══════════════════════════════════════════════════════════════
 *  PUBLIC API
 * ═══════════════════════════════════════════════════════════════ */
void FW_BuImageRx_init(void)
{
    fwBuState         = FW_BU_RX_IDLE;
    fwBuRingHead      = 0;
    fwBuRingTail      = 0;
    fwBuPendingCmd    = 0;
    fwBuFramesReceived = 0;
    fwBuBurstCount    = 0;
    fwBuWriteAddr     = FW_BANK1_START;
    memset(&fwBuImageInfo, 0, sizeof(fwBuImageInfo));
}

void FW_BuImageRx_isrOnData(const MCAN_RxBufElement *rxElem)
{
    if (fwBuState != FW_BU_RX_RECEIVING)
        return;

    uint16_t nextHead = (fwBuRingHead + 1U) & FW_BU_RX_RING_MASK;
    if (nextHead == fwBuRingTail)
        return;  /* ring full — drop, sender will retry */

    memcpy(fwBuRingBuf[fwBuRingHead].data, rxElem->data, FW_BU_FRAME_SIZE);
    fwBuRingHead = nextHead;
}

void FW_BuImageRx_isrOnCommand(const MCAN_RxBufElement *rxElem)
{
    memcpy(&fwBuCmdMsg, rxElem, sizeof(MCAN_RxBufElement));
    fwBuPendingCmd = rxElem->data[0];
}

FW_BuRxState FW_BuImageRx_process(void)
{
    /* 1. Handle pending command */
    uint8_t cmd = fwBuPendingCmd;
    if (cmd != 0U)
    {
        fwBuPendingCmd = 0U;

        switch (cmd)
        {
            case CMD_BU_FW_START:     FW_Bu_handleStart();    break;
            case CMD_BU_FW_HEADER:    FW_Bu_handleHeader();   break;
            case CMD_BU_FW_COMPLETE:  FW_Bu_handleComplete(); break;
            case CMD_BU_FW_ABORT:     FW_Bu_handleAbort();    break;
            case CMD_BU_FW_GET_STATE: FW_Bu_handleGetState(); break;
            default: break;
        }
    }

    /* 2. Drain ring buffer -> flash while receiving */
    if (fwBuState == FW_BU_RX_RECEIVING)
    {
        while (fwBuRingTail != fwBuRingHead)
        {
            /* Late retransmission past the end of the image:
             * silently drop it. Re-send the final ACK so the sender
             * stops retrying, but DO NOT advance the write pointer or
             * write past the staged image. Writing past the image
             * would corrupt the next sector AND leave the receiver
             * sending duplicate ACKs that the host would later mistake
             * for the verify response. */
            if (fwBuFramesReceived >= fwBuImageInfo.totalFrames)
            {
                fwBuRingTail = (fwBuRingTail + 1U) & FW_BU_RX_RING_MASK;
                FW_Bu_sendResponse(RESP_BU_FW_ACK,
                                   (uint16_t)(fwBuFramesReceived - 1U));
                continue;
            }

            EALLOW;
            FW_Bu_writeFrameToFlash(fwBuRingBuf[fwBuRingTail].data);
            EDIS;
            fwBuRingTail = (fwBuRingTail + 1U) & FW_BU_RX_RING_MASK;

            fwBuFramesReceived++;
            fwBuBurstCount++;

            if (fwBuBurstCount >= FW_BU_BURST_SIZE)
            {
                FW_Bu_sendResponse(RESP_BU_FW_ACK,
                                   (uint16_t)(fwBuFramesReceived - 1U));
                fwBuBurstCount = 0U;
            }
        }

        if (fwBuFramesReceived >= fwBuImageInfo.totalFrames &&
            fwBuBurstCount > 0U)
        {
            FW_Bu_sendResponse(RESP_BU_FW_ACK,
                               (uint16_t)(fwBuFramesReceived - 1U));
            fwBuBurstCount = 0U;
        }
    }

    return fwBuState;
}

FW_BuRxState FW_BuImageRx_getState(void)
{
    return fwBuState;
}

const FW_BuImageInfo *FW_BuImageRx_getImageInfo(void)
{
    return &fwBuImageInfo;
}

/* ═══════════════════════════════════════════════════════════════
 *  COMMAND HANDLERS
 * ═══════════════════════════════════════════════════════════════ */
static void FW_Bu_handleStart(void)
{
    fwBuState     = FW_BU_RX_ERASING;
    updateFailed  = false;

    /* Re-init Flash API and select bank 0 (single API context for the
     * F28P55x — bank parameter is selected per-erase via the address). */
    EALLOW;
    Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS,
                       DEVICE_SYSCLK_FREQ / 1000000U);
    Fapi_setActiveFlashBank(Fapi_FlashBank0);

    FW_eraseFlashRange(FW_BANK1_START, FW_BANK1_SECTORS);
    EDIS;

    if (updateFailed)
    {
        FW_Bu_sendResponse(RESP_BU_FW_NAK, 0);
        fwBuState = FW_BU_RX_IDLE;
        return;
    }

    fwBuWriteAddr      = FW_BANK1_START;
    fwBuFramesReceived = 0;
    fwBuBurstCount     = 0;
    fwBuRingHead       = 0;
    fwBuRingTail       = 0;
    memset(&fwBuImageInfo, 0, sizeof(fwBuImageInfo));

    fwBuState = FW_BU_RX_WAITING_HEADER;
    FW_Bu_sendResponse(RESP_BU_FW_ACK, 0);
}

static void FW_Bu_handleHeader(void)
{
    if (fwBuState != FW_BU_RX_WAITING_HEADER)
    {
        FW_Bu_sendResponse(RESP_BU_FW_NAK, 0);
        return;
    }

    /* Header layout (matches fw_image_rx.c convention):
     *   payload[4..63] of CMD frame:
     *     bytes 4..7   imageSize  (LE u32)
     *     bytes 8..11  imageCRC   (LE u32)
     *     bytes 12..15 version    (LE u32)
     *     bytes 24..27 totalFrames(LE u32)
     */
    const uint8_t *p = &fwBuCmdMsg.data[4];

    fwBuImageInfo.imageSize   = (uint32_t)p[4]  | ((uint32_t)p[5]  << 8)  |
                                ((uint32_t)p[6]  << 16) | ((uint32_t)p[7]  << 24);
    fwBuImageInfo.imageCRC    = (uint32_t)p[8]  | ((uint32_t)p[9]  << 8)  |
                                ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
    fwBuImageInfo.version     = (uint32_t)p[12] | ((uint32_t)p[13] << 8)  |
                                ((uint32_t)p[14] << 16) | ((uint32_t)p[15] << 24);
    fwBuImageInfo.totalFrames = (uint32_t)p[24] | ((uint32_t)p[25] << 8)  |
                                ((uint32_t)p[26] << 16) | ((uint32_t)p[27] << 24);

    fwBuState = FW_BU_RX_RECEIVING;
    FW_Bu_sendResponse(RESP_BU_FW_ACK, 0);
}

static void FW_Bu_handleComplete(void)
{
    fwBuState = FW_BU_RX_VERIFYING;

    uint32_t computedCRC = FW_Bu_calculateCRC32(FW_BANK1_START,
                                                fwBuImageInfo.imageSize);

    if (computedCRC == fwBuImageInfo.imageCRC)
    {
        fwBuState = FW_BU_RX_READY;   /* Bank 1 holds verified BU image */
        FW_Bu_sendResponse(RESP_BU_FW_CRC_PASS, 0);
    }
    else
    {
        fwBuState = FW_BU_RX_IDLE;
        FW_Bu_sendResponse(RESP_BU_FW_CRC_FAIL, 0);
    }
}

static void FW_Bu_handleAbort(void)
{
    fwBuState         = FW_BU_RX_IDLE;
    fwBuPendingCmd    = 0U;
    fwBuRingHead      = 0U;
    fwBuRingTail      = 0U;
    fwBuFramesReceived = 0U;
    fwBuBurstCount    = 0U;
    memset(&fwBuImageInfo, 0, sizeof(fwBuImageInfo));
    FW_Bu_sendResponse(RESP_BU_FW_ACK, 0);
}

static void FW_Bu_handleGetState(void)
{
    FW_Bu_sendStateResponse();
}

/* ═══════════════════════════════════════════════════════════════
 *  FLASH WRITE — one 64-byte CAN-FD frame = 4 x 8-word (512-bit) writes
 * ═══════════════════════════════════════════════════════════════ */
#pragma CODE_SECTION(FW_Bu_writeFrameToFlash, ".TI.ramfunc");
static void FW_Bu_writeFrameToFlash(const uint8_t *frameData)
{
    Fapi_StatusType      oReturnCheck;
    Fapi_FlashStatusType oFlashStatus;
    uint16_t wordBuf[8];
    uint16_t chunk, w;

    for (chunk = 0; chunk < 4; chunk++)
    {
        const uint8_t *src = frameData + (chunk * 16);
        for (w = 0; w < 8; w++)
            wordBuf[w] = (uint16_t)src[w * 2] |
                         ((uint16_t)src[w * 2 + 1] << 8);

        ClearFSMStatus();

        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTA, 0x00000000U);
        Fapi_setupBankSectorEnable(
            FLASH_WRAPPER_PROGRAM_BASE + FLASH_O_CMDWEPROTB, 0x00000000U);

        oReturnCheck = Fapi_issueProgrammingCommand(
            (uint32_t *)fwBuWriteAddr, wordBuf, 8, 0, 0,
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

        fwBuWriteAddr += 8U;   /* advance by 8 16-bit words */
    }
}

/* ═══════════════════════════════════════════════════════════════
 *  CRC32 over flash region
 * ═══════════════════════════════════════════════════════════════ */
static uint32_t FW_Bu_calculateCRC32(uint32_t startAddr, uint32_t numBytes)
{
    uint32_t crc = 0xFFFFFFFFU;
    volatile uint16_t *wordPtr = (volatile uint16_t *)startAddr;
    uint32_t numWords = (numBytes + 1U) / 2U;
    uint32_t i;

    for (i = 0; i < numWords; i++)
    {
        uint16_t word = wordPtr[i];
        crc = (crc >> 8) ^ fwBuCrc32Table[(crc ^ (word & 0xFF)) & 0xFF];
        crc = (crc >> 8) ^ fwBuCrc32Table[(crc ^ (word >> 8)) & 0xFF];
    }
    return crc ^ 0xFFFFFFFFU;
}

/* ═══════════════════════════════════════════════════════════════
 *  CAN TX — response back to M-Board
 * ═══════════════════════════════════════════════════════════════ */
static void FW_Bu_sendResponse(uint8_t respCode, uint16_t seq)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)FW_BU_RESP_CAN_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;       /* 64 bytes */
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xBCU;

    TxMsg.data[0] = respCode;
    TxMsg.data[1] = 0x01;                  /* src = S-Board */
    TxMsg.data[2] = (uint8_t)(seq & 0xFFU);
    TxMsg.data[3] = (uint8_t)((seq >> 8) & 0xFFU);

    /* TX buffer 3 — buffers 0..2 are already used by ops/self-update */
    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 3U, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 3U);
    canTxWaitComplete(CAN_BU_BASE);
}

static void FW_Bu_sendStateResponse(void)
{
    MCAN_TxBufElement TxMsg;
    memset(&TxMsg, 0, sizeof(MCAN_TxBufElement));

    TxMsg.id  = ((uint32_t)FW_BU_RESP_CAN_ID) << 18U;
    TxMsg.brs = 0x1;
    TxMsg.dlc = 15;
    TxMsg.fdf = 0x1;
    TxMsg.efc = 1U;
    TxMsg.mm  = 0xBCU;

    TxMsg.data[0] = RESP_BU_FW_STATE;
    TxMsg.data[1] = (uint8_t)fwBuState;
    TxMsg.data[2] = (uint8_t)(fwBuFramesReceived & 0xFFU);
    TxMsg.data[3] = (uint8_t)((fwBuFramesReceived >> 8) & 0xFFU);
    TxMsg.data[4] = (uint8_t)(fwBuWriteAddr        & 0xFFU);
    TxMsg.data[5] = (uint8_t)((fwBuWriteAddr >> 8) & 0xFFU);
    TxMsg.data[6] = (uint8_t)((fwBuWriteAddr >> 16)& 0xFFU);
    TxMsg.data[7] = (uint8_t)((fwBuWriteAddr >> 24)& 0xFFU);
    TxMsg.data[8]  = (uint8_t)(fwBuImageInfo.imageSize        & 0xFFU);
    TxMsg.data[9]  = (uint8_t)((fwBuImageInfo.imageSize >> 8) & 0xFFU);
    TxMsg.data[10] = (uint8_t)((fwBuImageInfo.imageSize >> 16)& 0xFFU);
    TxMsg.data[11] = (uint8_t)((fwBuImageInfo.imageSize >> 24)& 0xFFU);
    TxMsg.data[12] = (uint8_t)(fwBuImageInfo.imageCRC        & 0xFFU);
    TxMsg.data[13] = (uint8_t)((fwBuImageInfo.imageCRC >> 8) & 0xFFU);
    TxMsg.data[14] = (uint8_t)((fwBuImageInfo.imageCRC >> 16)& 0xFFU);
    TxMsg.data[15] = (uint8_t)((fwBuImageInfo.imageCRC >> 24)& 0xFFU);

    MCAN_writeMsgRam(CAN_BU_BASE, MCAN_MEM_TYPE_BUF, 3U, &TxMsg);
    MCAN_txBufAddReq(CAN_BU_BASE, 3U);
    canTxWaitComplete(CAN_BU_BASE);
}
