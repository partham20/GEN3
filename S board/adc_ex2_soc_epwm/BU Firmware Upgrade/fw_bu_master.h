/*
 * fw_bu_master.h
 *
 *  S-Board side of the BU OTA pipeline — sender / master.
 *
 *  Once Bank 1 holds a verified BU firmware image (state ==
 *  FW_BU_RX_READY in fw_bu_image_rx), call FW_BuMaster_start() with
 *  the target BU board ID. The master then drives a state machine
 *  that streams Bank 1 contents to that BU board over MCANA, asks
 *  it to verify, and finally tells it to activate (reboot into the
 *  new image).
 *
 *  CAN IDs on MCANA (S-Board <-> BU):
 *      0x30  FW data       (S-Board -> BU)
 *      0x31  FW command    (S-Board -> BU)
 *      0x32  FW response   (BU -> S-Board)
 *
 *  Command codes ride in byte 0 of the data payload; byte 1 carries
 *  the target board ID (0xFF for broadcast). This means a single set
 *  of CAN IDs serves all 12 BU boards — a board ignores frames that
 *  are not addressed to it.
 */

#ifndef FW_BU_MASTER_H
#define FW_BU_MASTER_H

#include "common.h"
#include "fw_upgrade_config.h"

/* ── CAN IDs (aliases over fw_upgrade_config.h names) ─────────── */
#define FW_BU_TX_DATA_CAN_ID    FW_BU_DATA_ID
#define FW_BU_TX_CMD_CAN_ID     FW_BU_CMD_ID
#define FW_BU_TX_RESP_CAN_ID    FW_BU_RESP_ID

#define BU_BROADCAST_ID         FW_BU_BROADCAST_TARGET

/* ── Protocol constants ───────────────────────────────────────── */
#define FW_BU_MASTER_BURST      FW_BU_BURST_SIZE
#define FW_BU_MASTER_PAYLOAD    FW_BU_PAYLOAD_BYTES
#define FW_BU_MASTER_RETRIES    FW_BU_BURST_RETRIES

/* Commands and responses (BU_CMD_FW_*, BU_RESP_FW_*) come directly
 * from fw_upgrade_config.h — no further aliasing needed. */

/* ── Master state machine ─────────────────────────────────────── */
typedef enum {
    FW_BU_M_IDLE,
    FW_BU_M_PREPARING,      /* waiting BU ACK after PREPARE   */
    FW_BU_M_SEND_HEADER,    /* sending image header           */
    FW_BU_M_SENDING_DATA,   /* streaming data frames          */
    FW_BU_M_VERIFYING,      /* waiting VERIFY response        */
    FW_BU_M_ACTIVATING,     /* sent ACTIVATE, awaiting reboot */
    FW_BU_M_DONE,           /* BU board reported BOOT_OK      */
    FW_BU_M_FAILED          /* terminal failure for this run  */
} FW_BuMasterState;

typedef enum {
    FW_BU_M_ERR_NONE = 0,
    FW_BU_M_ERR_NO_IMAGE,
    FW_BU_M_ERR_NAK,
    FW_BU_M_ERR_VERIFY,
    FW_BU_M_ERR_TIMEOUT,
    FW_BU_M_ERR_RETRIES_EXHAUSTED
} FW_BuMasterError;

/* ── Public API ───────────────────────────────────────────────── */

void FW_BuMaster_init(void);

/* Begin pushing the staged Bank 1 image to BU board `boardId`. Returns
 * 0 if accepted, nonzero if the prerequisite (verified image in Bank 1)
 * is not satisfied or the master is busy. */
uint16_t FW_BuMaster_start(uint8_t boardId);

/* Drive the state machine. Call from the S-Board main loop. */
FW_BuMasterState FW_BuMaster_process(void);

/* ISR hook — call from the MCAN ISR when a frame on FW_BU_TX_RESP_CAN_ID
 * is received from the BU board. Updates internal latch the main loop
 * inspects on the next FW_BuMaster_process() call. */
void FW_BuMaster_isrOnResponse(const MCAN_RxBufElement *rxElem);

/* Abort the in-flight transfer, send ABORT to the BU. */
void FW_BuMaster_abort(void);

FW_BuMasterState FW_BuMaster_getState(void);
FW_BuMasterError FW_BuMaster_getError(void);

/* Progress accessors — used by the M-Board status reply handler. */
uint8_t  FW_BuMaster_getTargetId(void);    /* 0xFF if idle */
uint32_t FW_BuMaster_getBytesSent(void);
uint32_t FW_BuMaster_getImageSize(void);
uint16_t FW_BuMaster_getBulkBitmap(void);  /* set when all-BUs sweep is wired */
uint8_t  FW_BuMaster_getLastCompleted(void);

/* Send a status reply frame on FW_BU_STATUS_REPLY_ID (0x04).
 * Called by the S-Board main loop in response to
 * CMD_BU_FW_STATUS_REQUEST (0x0F) from the M-Board. */
void FW_BuMaster_sendStatusReply(void);

#endif /* FW_BU_MASTER_H */
