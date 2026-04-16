/*
 * fw_update_bu.h
 *
 *  BU-Board side of the firmware OTA pipeline.
 *
 *  Listens for FW commands and data from the S-Board on MCANA, writes
 *  the incoming image into the BU board's flash Bank 2 (the staging
 *  area, distinct from Bank 0 which holds the running app and the
 *  boot manager). On successful CRC verification it writes the boot
 *  flag to Bank 3 and resets the device — the boot manager (loaded
 *  separately via JTAG, not OTA) then copies Bank 2 -> Bank 0 and
 *  jumps to the new application.
 *
 *  CAN IDs (must be added as filter entries in MCANConfig() — they
 *  are NOT in the existing 5/6/7 set used for voltage data):
 *      0x30  FW data       (S-Board -> BU)
 *      0x31  FW command    (S-Board -> BU)
 *      0x32  FW response   (BU -> S-Board)
 *
 *  Required project additions before this module compiles cleanly:
 *      - FAPI_F28P55x_EABI_v4.00.00.lib  (already in S-Board project)
 *      - flash_programming_f28p55x.h     (header from C2000Ware)
 *      - FlashTech_F28P55x_C28x.h        (header from C2000Ware)
 *      - .TI.ramfunc section in linker .cmd file (BU board linker
 *        currently has no entry for this — needs to mirror S-Board)
 *      - Boot manager flashed into Bank 0 sectors 0..3 via JTAG
 */

#ifndef FW_UPDATE_BU_H
#define FW_UPDATE_BU_H

#include <stdint.h>
#include <stdbool.h>
#include "driverlib.h"
#include "device.h"
#include "fw_upgrade_config.h"

/* ── CAN IDs (aliases over fw_upgrade_config.h names) ─────────── */
#define BU_FW_DATA_CAN_ID       FW_BU_DATA_ID
#define BU_FW_CMD_CAN_ID        FW_BU_CMD_ID
#define BU_FW_RESP_CAN_ID       FW_BU_RESP_ID

#define BU_FW_BROADCAST_ID      FW_BU_BROADCAST_TARGET

/* Commands (BU_CMD_FW_*) and responses (BU_RESP_FW_*) come directly
 * from fw_upgrade_config.h — no further aliasing needed. */

/* ── Protocol constants ───────────────────────────────────────── */
#define BU_FW_BURST_SIZE        FW_BU_BURST_SIZE
#define BU_FW_PAYLOAD_BYTES     FW_BU_PAYLOAD_BYTES
#define BU_FW_RX_RING_SIZE      FW_BU_RECV_RX_RING_SIZE
#define BU_FW_RX_RING_MASK      (BU_FW_RX_RING_SIZE - 1U)

/* ── Flash layout (aliases over config) ───────────────────────── */
#define BU_BANK2_START          FW_BU_BANK2_START
#define BU_BANK2_SECTORS        FW_BU_BANK2_SECTORS
#define BU_BANK3_FLAG_ADDR      FW_BU_BANK3_FLAG_ADDR
#define BU_BOOT_FLAG_PENDING    FW_BOOT_FLAG_PENDING
#define BU_BOOT_FLAG_CRC_VALID  FW_BOOT_FLAG_CRC_VALID

/* ── Receiver state machine ───────────────────────────────────── */
typedef enum {
    BU_FW_IDLE,
    BU_FW_PREPARING,
    BU_FW_WAITING_HEADER,
    BU_FW_RECEIVING,
    BU_FW_VERIFYING,
    BU_FW_ACTIVATING
} BU_FwState;

typedef struct {
    uint32_t imageSize;     /* bytes (raw, host-side computed) */
    uint32_t imageCRC;      /* CRC32 of image */
    uint32_t version;       /* firmware version */
} BU_FwImageInfo;

/* ── Public API ───────────────────────────────────────────────── */

/* Reset state. Call once after MCANConfig(). The CAN filter for
 * IDs 0x30 / 0x31 must already be installed in MCANConfig(). */
void BU_Fw_init(uint8_t myBoardId);

/* ISR hook — call from MCANIntr1ISR when a frame whose ID matches
 * BU_FW_DATA_CAN_ID or BU_FW_CMD_CAN_ID is read out of the FIFO /
 * dedicated buffer. Both calls are very fast (memcpy + flag set). */
void BU_Fw_isrOnFrame(uint32_t canId, const MCAN_RxBufElement *rxElem);

/* Drive from the BU main loop. Drains the ring buffer to flash and
 * acts on any pending command. While in BU_FW_RECEIVING the caller
 * should suspend ADC / power-calculation processing — flash writes
 * cannot be safely interleaved with the 5.12 kHz sample loop. */
BU_FwState BU_Fw_process(void);

BU_FwState BU_Fw_getState(void);
bool       BU_Fw_isBusy(void);   /* true when not IDLE */

#endif /* FW_UPDATE_BU_H */
