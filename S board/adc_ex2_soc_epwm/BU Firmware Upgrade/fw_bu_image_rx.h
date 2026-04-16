/*
 * fw_bu_image_rx.h
 *
 *  S-Board side of the BU firmware OTA pipeline.
 *
 *  Responsibility: receive a BU board's .bin file from the M-Board (or
 *  PC test tool) over MCANB / MCANA and store it in S-Board flash Bank 1
 *  (0x0A0000). Bank 1 is dedicated as the BU firmware staging area —
 *  it is NEVER copied into Bank 0 of the S-Board; instead the S-Board
 *  later re-transmits its contents to the BU boards via fw_bu_master.
 *
 *  Distinct from fw_image_rx.* (which stages the S-Board's OWN firmware
 *  in Bank 2 for self-update via the boot manager).
 *
 *  CAN IDs (chosen to NOT collide with existing 1-8 used by ops &
 *  S-Board self-update):
 *      0x18  BU FW data       (M-Board -> S-Board)
 *      0x19  BU FW command    (M-Board -> S-Board)
 *      0x1A  BU FW response   (S-Board -> M-Board)
 */

#ifndef FW_BU_IMAGE_RX_H
#define FW_BU_IMAGE_RX_H

#include "common.h"
#include "flash_driver.h"
#include "fw_upgrade_config.h"

/* ── CAN IDs (pulled from fw_upgrade_config.h) ────────────────── */
#define FW_BU_DATA_CAN_ID       FW_BU_STAGE_DATA_ID
#define FW_BU_CMD_CAN_ID        FW_BU_STAGE_CMD_ID
#define FW_BU_RESP_CAN_ID       FW_BU_STAGE_RESP_ID

/* ── Command codes (host → S-Board) ───────────────────────────── */
#define CMD_BU_FW_START         BU_STAGE_CMD_FW_START
#define CMD_BU_FW_HEADER        BU_STAGE_CMD_FW_HEADER
#define CMD_BU_FW_COMPLETE      BU_STAGE_CMD_FW_COMPLETE
#define CMD_BU_FW_ABORT         BU_STAGE_CMD_FW_ABORT
#define CMD_BU_FW_GET_STATE     BU_STAGE_CMD_FW_GET_STATE

/* ── Response codes (S-Board → host) ──────────────────────────── */
#define RESP_BU_FW_ACK          BU_STAGE_RESP_FW_ACK
#define RESP_BU_FW_NAK          BU_STAGE_RESP_FW_NAK
#define RESP_BU_FW_CRC_PASS     BU_STAGE_RESP_FW_CRC_PASS
#define RESP_BU_FW_CRC_FAIL     BU_STAGE_RESP_FW_CRC_FAIL
#define RESP_BU_FW_STATE        BU_STAGE_RESP_FW_STATE

/* ── Protocol constants ───────────────────────────────────────── */
/* FW_BU_BURST_SIZE comes straight from fw_upgrade_config.h; same
 * value used on both the host→S-Board and S-Board→BU hops. */
#define FW_BU_FRAME_SIZE        64U     /* CAN-FD payload bytes — fixed */
#define FW_BU_RX_RING_SIZE      FW_BU_STAGE_RX_RING_SIZE
#define FW_BU_RX_RING_MASK      (FW_BU_RX_RING_SIZE - 1U)

/* ── Flash target — Bank 1 holds BU firmware staging on the S-Board ── */
#define FW_BANK1_START          FW_S_BANK1_START
#define FW_BANK1_SECTORS        FW_S_BANK1_SECTORS

/* ── State machine ────────────────────────────────────────────── */
typedef enum {
    FW_BU_RX_IDLE,
    FW_BU_RX_ERASING,
    FW_BU_RX_WAITING_HEADER,
    FW_BU_RX_RECEIVING,
    FW_BU_RX_VERIFYING,
    FW_BU_RX_READY               /* Bank 1 holds valid verified image */
} FW_BuRxState;

typedef struct {
    uint32_t imageSize;          /* size in bytes (padded to 16-byte chunks) */
    uint32_t imageCRC;           /* CRC32 of raw image */
    uint32_t version;            /* firmware version */
    uint32_t totalFrames;        /* expected data-frame count */
} FW_BuImageInfo;

typedef struct {
    uint8_t data[FW_BU_FRAME_SIZE];
} FW_BuRxFrame;

/* ── Public API ───────────────────────────────────────────────── */

/* Reset state. Filters for IDs 0x18 / 0x19 must already exist in
 * MCANAConfig() (or the appropriate CAN init for whichever bus the
 * M-Board uses to deliver the BU image). */
void FW_BuImageRx_init(void);

/* ISR hooks — called from MCAN ISR when a frame on the matching ID
 * lands in a dedicated RX buffer. Both are very fast. */
void FW_BuImageRx_isrOnData(const MCAN_RxBufElement *rxElem);
void FW_BuImageRx_isrOnCommand(const MCAN_RxBufElement *rxElem);

/* Drive from main loop — drains ring buffer to flash and handles
 * the most-recently received command. Returns current state. */
FW_BuRxState FW_BuImageRx_process(void);

/* Read-only accessors used by fw_bu_master so it knows what to send. */
FW_BuRxState         FW_BuImageRx_getState(void);
const FW_BuImageInfo *FW_BuImageRx_getImageInfo(void);

#endif /* FW_BU_IMAGE_RX_H */
