/*
 * fw_mode.h — S-Board FW Update Mode control
 *
 * Coordinates the system-wide transition between two steady states:
 *
 *   NORMAL   — 3-phase ADC + voltage TX + BU data collection + M-Board reports
 *   FW_MODE  — all real-time work paused; only the OTA protocol + a 1 Hz
 *              heartbeat run. S-Board fans ENTER/EXIT out to every BU-Board
 *              on MCANA so the whole system stays in lock-step.
 *
 * Trigger flow:
 *   M-Board → S-Board: CMD_ENTER_FW_MODE on MCANB (ID 7)
 *   S-Board: stops ePWM7 / DMA / ADC-ISRs / periodic TX
 *   S-Board → BU-Boards: BU_CMD_ENTER_FW_MODE on MCANA (broadcast ID 4)
 *   ... OTA protocol runs ...
 *   M-Board → S-Board: CMD_EXIT_FW_MODE
 *   S-Board → BU-Boards: BU_CMD_EXIT_FW_MODE
 *   S-Board + BU-Boards: restart peripherals, resume normal work
 *
 * Status reporting:
 *   - Push:  each board broadcasts RESP_FW_RESULT once on boot (after startup)
 *   - Pull:  M-Board sends CMD_FW_STATUS_REQ; S-Board queries every BU
 *            (BU_CMD_FW_STATUS_REQ), aggregates, returns RESP_FW_SUMMARY.
 */

#ifndef FW_MODE_H
#define FW_MODE_H

#include "common.h"

typedef enum {
    FW_MODE_NORMAL = 0,
    FW_MODE_ACTIVE
} FwModeState;

/* BU-Board result record held by S-Board while aggregating. */
typedef struct {
    uint8_t   valid;       /* 1 if we received a result this session */
    uint8_t   bu_id;       /* 10 + dip value (CAN ID) */
    uint8_t   status;      /* FW_RESULT_* */
    uint16_t  version;
    uint32_t  crc;
} FwBuResult;

/* ── Public API ──────────────────────────────────────────────── */

/* Called once from startup() after all peripherals are up. */
void FwMode_init(void);

/* Main-loop hook — call every iteration. Emits the 1 Hz FW-mode
 * heartbeat when active. No-op in NORMAL. */
void FwMode_tick(void);

/* Query current mode. */
FwModeState FwMode_getState(void);
static inline uint8_t FwMode_isActive(void) {
    return (uint8_t)(FwMode_getState() == FW_MODE_ACTIVE);
}

/* ISR-safe entry: inspects data[0] and, if it is one of ENTER/EXIT/
 * STATUS_REQ, latches the command for main-loop processing.
 * Returns 1 if the command was consumed (caller should not forward
 * to any other handler), 0 otherwise. */
uint8_t FwMode_isrOnCommand(const uint8_t *data);

/* ISR-safe: called from MCANA RX when a BU-Board sends RESP_FW_RESULT.
 * Snapshots into the result table — no TX from ISR. */
void FwMode_recordBuResult(const uint8_t *data);

/* Emitted once from startup() after peripherals come up — tells
 * the M-Board that this S-Board is alive and which version it is. */
void FwMode_sendBootStatus(void);

#endif /* FW_MODE_H */
