/*
 * fw_image_rx.h
 *
 *  Created on: 03-Apr-2026
 *      Author: Parthasarathy.M
 *
 *  Receives firmware binary over CAN-FD, writes to Bank 2 flash.
 *  Scope: M-Board → S-Board transfer only.
 */

#ifndef FW_IMAGE_RX_H
#define FW_IMAGE_RX_H

#include "common.h"
#include "flash_driver.h"

/* ── CAN IDs (no conflict with existing 1-5) ─────────────────── */
#define FW_DATA_CAN_ID          6       /* Data:     M-Board → S-Board */
#define FW_CMD_CAN_ID           7       /* Command:  M-Board → S-Board */
#define FW_RESP_CAN_ID          8       /* Response: S-Board → M-Board */

/* ── Command codes ────────────────────────────────────────────── */
#define CMD_FW_START            0x30    /* Erase staging bank, prepare */
#define CMD_FW_HEADER           0x31    /* Image metadata (size, CRC) */
#define CMD_FW_COMPLETE         0x33    /* All data sent, verify CRC */

/* ── Extended debug/control commands (GUI tools) ─────────────── */
#define CMD_READ_FLAG           0x34    /* Read boot flag → respond with values */
#define CMD_CLEAR_FLAG          0x35    /* Erase Bank 3 sector 0 (clear flag) */
#define CMD_RESET_DEVICE        0x36    /* Reset MCU immediately */
#define CMD_READ_FLASH          0x37    /* Read 8 words from addr → respond */
#define CMD_COMPUTE_CRC         0x38    /* CRC32 over range → respond */
#define CMD_ERASE_SECTOR        0x39    /* Erase one sector at addr */
#define CMD_WRITE_FLAG          0x3A    /* Manually write boot flag */
#define CMD_GET_STATE           0x3B    /* Report current FW_RxState */

/* ── Extended response codes ─────────────────────────────────── */
#define RESP_FLAG_DATA          0x29    /* Boot flag values */
#define RESP_FLASH_DATA         0x2A    /* 8 words from flash */
#define RESP_CRC_DATA           0x2B    /* CRC32 result */
#define RESP_STATE_DATA         0x2C    /* Current state */

/* ── Response codes ───────────────────────────────────────────── */
#define RESP_FW_ACK             0x25
#define RESP_FW_NAK             0x26
#define RESP_FW_CRC_PASS        0x27
#define RESP_FW_CRC_FAIL        0x28

/* ── Protocol constants ───────────────────────────────────────── */
#define FW_BURST_SIZE           16      /* ACK after every 16 data frames */
#define FW_FRAME_SIZE           64      /* bytes per CAN-FD data frame */
#define FW_RX_RING_SIZE         64      /* ring buffer depth (power of 2) */
#define FW_RX_RING_MASK         (FW_RX_RING_SIZE - 1)

/* ── Flash target ─────────────────────────────────────────────── */
#define FW_BANK2_START          0x0C0000U
#define FW_BANK2_SECTORS        128

/* ── Boot flag (Bank 3, sector 0) — must match boot_manager.c ── */
#define FW_BANK3_FLAG_ADDR      0x0E0000U
#define FW_FLAG_UPDATE_PENDING  0xA5A5U
#define FW_FLAG_CRC_VALID       0x5A5AU

/* ── State machine ────────────────────────────────────────────── */
typedef enum {
    FW_IDLE,                /* Waiting for CMD_FW_START */
    FW_ERASING,             /* Erasing Bank 2 */
    FW_WAITING_HEADER,      /* Erased, waiting for image header */
    FW_RECEIVING,           /* Receiving data, writing to flash */
    FW_VERIFYING            /* CRC verification */
} FW_RxState;

/* ── Image info (parsed from CMD_FW_HEADER) ───────────────────── */
typedef struct {
    uint32_t imageSize;     /* padded size in bytes */
    uint32_t imageCRC;      /* CRC32 of image data */
    uint32_t version;       /* firmware version */
    uint32_t totalFrames;   /* expected data frame count */
} FW_ImageInfo;

/* ── Ring buffer entry ────────────────────────────────────────── */
typedef struct {
    uint8_t data[FW_FRAME_SIZE];
} FW_RxFrame;

/* ── Public API ───────────────────────────────────────────────── */

/* Call once at startup to add MCANA filters for ID 6 and ID 7 */
void FW_ImageRx_init(void);

/* Called from MCANA ISR when data frame (ID 6) arrives.
 * Copies 64 bytes into ring buffer — no flash ops, very fast. */
void FW_ImageRx_isrOnData(const MCAN_RxBufElement *rxElem);

/* Called from MCANA ISR when command frame (ID 7) arrives.
 * Just sets a flag — main loop handles it. */
void FW_ImageRx_isrOnCommand(const MCAN_RxBufElement *rxElem);

/* Called from main loop — handles commands, drains ring buffer
 * to flash, sends ACKs. Returns current state. */
FW_RxState FW_ImageRx_process(void);

/* Write boot flag to Bank 3 and reset MCU.
 * Called after CRC verification passes — triggers boot manager
 * to copy Bank 2 → Bank 0 on next boot. */
void FW_triggerUpdate(void);

#endif /* FW_IMAGE_RX_H */