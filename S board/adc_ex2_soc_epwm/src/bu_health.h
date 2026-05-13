/*
 * bu_health.h -- per-BU-board health monitor.
 *
 * Reports to the M-Board, on FW_MODE_RESP_CAN_ID (0x008), three
 * conditions about the BU fleet that the M-Board needs to act on:
 *
 *   1. Any BU is running a firmware version different from the rest
 *      (or different from the S-Board's own APP_VERSION).
 *   2. Two BU boards share the same CAN ID (DIP-switch collision).
 *      Detected during discovery via the existing duplicateIDFound
 *      flag in bu_board.c.
 *   3. A BU that was discovered active has stopped sending runtime
 *      data for longer than BU_HEALTH_DEAD_MS.
 *
 * One CAN-FD frame is emitted every BU_HEALTH_REPORT_MS, opcode
 * RESP_BU_HEALTH = 0x42.  Frame layout:
 *
 *   byte 0       : 0x42                          (RESP_BU_HEALTH)
 *   byte 1       : num present                   (popcount of present_bm)
 *   byte 2       : num alive                     (popcount of alive_bm)
 *   byte 3       : expected version low  (APP_VERSION & 0xFF)
 *   byte 4       : expected version high ((APP_VERSION >> 8) & 0xFF)
 *   byte 5       : flag bits
 *                    bit 0 = any version mismatch
 *                    bit 1 = any duplicate CAN ID
 *                    bit 2 = any dead BU (was present, now silent)
 *   bytes 6..7   : present bitmap         (LE u16, bit i = slot i)
 *   bytes 8..9   : alive bitmap           (LE u16)
 *   bytes 10..11 : version mismatch bm    (LE u16)
 *   bytes 12..13 : duplicate bm           (LE u16)
 *   bytes 14..15 : dead bitmap            (LE u16) [present & !alive]
 *   bytes 16..39 : per-BU detail, 2 bytes per slot:
 *                    byte 16+2i = version
 *                    byte 17+2i = status (0=absent / 1=alive /
 *                                         2=dead    / 3=duplicate)
 *
 * Slot i corresponds to BU board id (FIRST_BU_ID + i), i.e. slot 0
 * = BU 11, slot 1 = BU 12, ... slot 11 = BU 22.
 */

#ifndef BU_HEALTH_H
#define BU_HEALTH_H

#include "common.h"

/* How recently a BU must have sent a runtime frame to count as alive. */
#define BU_HEALTH_DEAD_MS         10000U
/* How often to emit RESP_BU_HEALTH on the M-Board bus. */
#define BU_HEALTH_REPORT_MS        5000U

/* Initialise internal state.  Call once at startup, after Board_init. */
void BuHealth_init(void);

/* Call from the BU-runtime-frame ingest path (runtimedata.c) for
 * every runtime data frame that passed the boardId range check.
 * Updates the BU's last_seen_ms and remembers its version byte. */
void BuHealth_onRuntimeFrame(uint32_t boardId, uint8_t version);

/* Call from the main loop on every iteration.  Internally rate-limits
 * to one RESP_BU_HEALTH transmission per BU_HEALTH_REPORT_MS. */
void BuHealth_tick(void);

#endif /* BU_HEALTH_H */
