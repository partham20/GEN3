/*
 * fw_upgrade_config.h
 *
 * SINGLE source of truth for every configurable parameter used by the
 * BU firmware OTA pipeline. This file is duplicated BYTE-FOR-BYTE in
 * both the S-Board and BU-Board projects â€” if you edit one, edit the
 * other to match (or they'll stop talking to each other).
 *
 * What lives here:
 *   - CAN interface + bit timing
 *   - CAN IDs for every frame type used by the BU OTA
 *   - Command / response byte codes
 *   - Burst, payload, retry, and timeout constants
 *   - Flash addresses (S-Board Bank 1 staging, BU Bank 2 staging,
 *     BU Bank 3 boot flag, BU application entry)
 *   - BU board ID range
 *   - M-Board â†’ S-Board command opcodes that trigger the pipeline
 *
 * Everything is a #define. Change a number, rebuild both projects,
 * and the whole pipeline respects the new value. No magic numbers
 * should exist in fw_bu_image_rx.*, fw_bu_master.*, or fw_update_bu.*.
 */

#ifndef FW_UPGRADE_CONFIG_H
#define FW_UPGRADE_CONFIG_H

#include "../can_config.h"

/* â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�
 *  CAN INTERFACE & BIT TIMING
 * â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•� */

/* Which MCAN module the BU OTA runs on. Both S-Board (sender) and
 * BU boards (receiver) must use the same one because they share the
 * bus physically. Configured via can_config.h (CAN_BU_BASE). */
#define FW_CAN_BASE                 CAN_BU_BASE

/* GPIO pin config â€” configured via can_config.h (CAN_BU_TX_PIN /
 * CAN_BU_RX_PIN). To change pins, edit can_config.h instead. */
#define FW_BU_MCAN_TX_PIN           CAN_BU_TX_PIN
#define FW_BU_MCAN_RX_PIN           CAN_BU_RX_PIN

/* MCAN module clock in MHz (after SysCtl_setMCANClk divisor).
 * SYSCLK 150 MHz / 5 = 30 MHz on the F28P55x default setup. */
#define FW_CAN_CLOCK_MHZ            30U

/* FD mode and bit-rate switching â€” both boards must agree. */
#define FW_CAN_FD_ENABLED           1U
#define FW_CAN_BRS_ENABLED          1U

/* Nominal (arbitration) phase â€” targets 500 kbps with 30 MHz clock:
 *   30 MHz / ((5+1) * (1 + (6+1) + (1+1))) = 30M / (6 * 10) = 500 kbps
 *   Sample point ~80%. */
#define FW_CAN_NOM_BRP              5U
#define FW_CAN_NOM_TSEG1            6U
#define FW_CAN_NOM_TSEG2            1U
#define FW_CAN_NOM_SJW              1U

/* Data (BRS) phase â€” targets 2 Mbps:
 *   30 MHz / ((0+1) * (1 + (10+1) + (2+1))) = 30M / (1 * 15) = 2 Mbps */
#define FW_CAN_DATA_BRP             0U
#define FW_CAN_DATA_TSEG1           10U
#define FW_CAN_DATA_TSEG2           2U
#define FW_CAN_DATA_SJW             2U

/* â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�
 *  CAN IDs  (S-Board â†” BU OTA â€” disjoint from existing traffic)
 *
 *  Existing bus usage (do NOT collide):
 *    0x01..0x05   operational (M-Board, voltage, etc.)
 *    0x06..0x08   S-Board self-update (fw_image_rx)
 *    0x0B..0x0D   discovery
 *    0x0B..0x16   BU board replies (11..22)
 *    0x18..0x1A   M-Board â†’ S-Board BU image staging
 * â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•� */

/* â”€â”€ Hop 1: Host (PC / M-Board) â†’ S-Board  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Used by fw_bu_image_rx on the S-Board to stage a BU .bin into
 * Bank 1 before fw_bu_master re-transmits it to a BU board. */
#define FW_BU_STAGE_DATA_ID         0x18U   /* host â†’ S-Board: data frames  */
#define FW_BU_STAGE_CMD_ID          0x19U   /* host â†’ S-Board: commands     */
#define FW_BU_STAGE_RESP_ID         0x1AU   /* S-Board â†’ host: responses    */

/* â”€â”€ Hop 2: S-Board â†’ BU-Board  â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Used by fw_bu_master (sender, on S-Board) and fw_update_bu
 * (receiver, on BU) to stream Bank 1 contents into BU Bank 2. */
#define FW_BU_DATA_ID               0x30U   /* S-Board â†’ BU:  data frames   */
#define FW_BU_CMD_ID                0x31U   /* S-Board â†’ BU:  commands      */
#define FW_BU_RESP_ID               0x32U   /* BU â†’ S-Board:  responses     */

/* Broadcast target â€” every BU sees it; addressing is done by
 * comparing data[1] against the BU's own board id. */
#define FW_BU_BROADCAST_TARGET      0xFFU

/* â”€â”€ S-Board â†’ M-Board: BU FW status reply channel â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€
 * Carries the response to CMD_BU_FW_STATUS_REQUEST (0x0F), which the
 * M-Board polls while a BU upgrade is in progress. Separate from the
 * ID 3 command channel (M-Board â†’ S-Board) so replies do not loop
 * back to the same filter. */
#define FW_BU_STATUS_REPLY_ID       0x04U

/* â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�
 *  COMMAND / RESPONSE CODES
 * â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•� */

/* â”€â”€ Hop 1: Host â†’ S-Board (staging into Bank 1) â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
#define BU_STAGE_CMD_FW_START       0x40U   /* Erase Bank 1, prepare */
#define BU_STAGE_CMD_FW_HEADER      0x41U   /* Image metadata        */
#define BU_STAGE_CMD_FW_COMPLETE    0x42U   /* End of data, verify   */
#define BU_STAGE_CMD_FW_ABORT       0x43U   /* Abort and reset state */
#define BU_STAGE_CMD_FW_GET_STATE   0x44U   /* Report rx state       */

#define BU_STAGE_RESP_FW_ACK        0x45U
#define BU_STAGE_RESP_FW_NAK        0x46U
#define BU_STAGE_RESP_FW_CRC_PASS   0x47U
#define BU_STAGE_RESP_FW_CRC_FAIL   0x48U
#define BU_STAGE_RESP_FW_STATE      0x49U

/* â”€â”€ Hop 2: S-Board â†’ BU-Board (streaming into BU Bank 2) â”€â”€â”€â”€â”€â”€â”€â”€â”€ */
#define BU_CMD_FW_PREPARE           0x50U   /* stop ops, erase Bank 2 staging */
#define BU_CMD_FW_HEADER            0x51U   /* image meta (size / CRC / version) */
#define BU_CMD_FW_DATA              0x52U   /* 60-byte payload (on data frame) */
#define BU_CMD_FW_VERIFY            0x53U   /* compute CRC32 over staging */
#define BU_CMD_FW_ACTIVATE          0x54U   /* write boot flag, reset */
#define BU_CMD_FW_ABORT             0x55U   /* abort, resume normal ops */

#define BU_RESP_FW_ACK              0x60U
#define BU_RESP_FW_NAK              0x61U
#define BU_RESP_FW_VERIFY_PASS      0x62U
#define BU_RESP_FW_VERIFY_FAIL      0x63U
#define BU_RESP_FW_BOOT_OK          0x64U

/* â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�
 *  PROTOCOL CONSTANTS
 * â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•� */

/* Burst size â€” number of data frames streamed before the sender
 * waits for an ACK. Higher = faster but more bus-contention per
 * failed burst retransmit. */
#define FW_BU_BURST_SIZE            16U

/* Payload bytes per data frame.
 *
 * Data frames carry RAW firmware bytes â€” bytes [0..63] are firmware,
 * no command / target / sequence header. This matches the S-Board
 * self-update format exactly and lets the BU receiver write a single
 * contiguous 4Ã—8-word (64-byte) flash chunk per frame with no
 * intra-frame padding (which used to corrupt the flash layout when
 * we tried to mix 60-byte payloads with 64-byte flash writes).
 *
 * Target-id filtering still happens on COMMAND frames (CAN ID 0x31).
 * Once a specific BU has accepted a PREPARE for itself it is the
 * only board in RECEIVING state, so any data frame on 0x30 is
 * implicitly for it. */
#define FW_BU_PAYLOAD_BYTES         64U

/* Retries per burst when the ACK window times out. */
#define FW_BU_BURST_RETRIES         3U

/* Phase timeouts (milliseconds). */
#define FW_BU_TMO_PREPARE_MS        5000U   /* erase Bank 2 is slow */
#define FW_BU_TMO_BURST_MS           500U   /* ACK window per burst */
#define FW_BU_TMO_VERIFY_MS         5000U   /* CRC32 over Bank 2    */
#define FW_BU_TMO_BOOT_MS          10000U   /* boot manager copy + reboot */

/* Inter-frame delay when the S-Board master streams data to a BU.
 *
 * The BU receiver uses dedicated MCAN RX buffers (1 message slot
 * each). The MCAN ISR runs from FLASH, so during a flash write
 * (each Fapi_issueProgrammingCommand locks the flash module for
 * roughly 300-500 us) the ISR is delayed. If two CAN frames land
 * in the same dedicated buffer during one of those locked windows,
 * the second OVERWRITES the first and the master never knows.
 *
 * 1000 us guarantees that two consecutive frames are never closer
 * than ~1.25 ms (256 us on the wire + 1000 us pad), which is safely
 * larger than one Fapi program command. Empirically this is the
 * same delay the GUI's BU Direct mode uses successfully.
 *
 * Set to 0 if you ever move the BU receiver to FIFO0 (which can
 * queue up to 21 messages in hardware). */
#define FW_BU_INTER_FRAME_DELAY_US  1000U

/* Ring buffer depth (ISR â†’ main loop hand-off). Must be a power of 2.
 * Separate numbers for each hop because the two sides have different
 * memory budgets and different upstream rates. */
#define FW_BU_STAGE_RX_RING_SIZE    64U   /* S-Board staging from host */
#define FW_BU_RECV_RX_RING_SIZE     32U   /* BU receiver from S-Board  */

/* â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�
 *  FLASH LAYOUT
 * â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•� */

/* S-Board side â€” Bank 1 is the staging area for a BU image delivered
 * by the host GUI. Never copied into S-Board Bank 0 â€” only re-
 * transmitted out to BU boards by fw_bu_master. */
#define FW_S_BANK1_START            0x0A0000UL
#define FW_S_BANK1_SECTORS          128U

/* BU-Board side â€” Bank 2 is the staging area for the new application
 * image pushed by the S-Board. The BU boot manager copies Bank 2 â†’
 * Bank 0 sectors 8..127 once the boot flag is set. */
#define FW_BU_BANK2_START           0x0C0000UL
#define FW_BU_BANK2_SECTORS         128U

/* BU-Board side â€” Bank 3 sector 0 holds the 8-word boot flag that
 * the boot manager checks on every reset. */
#define FW_BU_BANK3_FLAG_ADDR       0x0E0000UL

/* BU-Board application entry point â€” must match boot_manager.c
 * APP_ENTRY_ADDR and the BU app linker file. Sector 8 of Bank 0.
 * (Sectors 0-7 are reserved for the boot manager, NEVER OTA'd.) */
#define FW_BU_APP_ENTRY             0x082000UL
#define FW_BU_APP_SECTOR_COUNT      120U   /* sectors 8..127 */

/* Boot flag magic words. */
#define FW_BOOT_FLAG_PENDING        0xA5A5U
#define FW_BOOT_FLAG_CRC_VALID      0x5A5AU

/* â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�
 *  BU BOARD ID RANGE
 * â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•� */

/* BU boards are addressed 11..22 on the CAN bus. Each BU reads its
 * own ID from DIP switches. fw_bu_master iterates this range when
 * doing a bulk update; fw_update_bu filters incoming frames by it. */
#define FW_BU_FIRST_ID              11U
#define FW_BU_LAST_ID               22U
#define FW_BU_MAX_COUNT             12U

/* BU-Board ID override for test setups.
 *
 * On a Launchpad dev board there are no DIP switches, so
 * readCANAddress() returns 0 by default. That breaks the target-id
 * filter in fw_update_bu.c â€” the S-Board sends frames with target
 * = 11..22 (or 0xFF broadcast), and a BU with fwMyBoardId = 0 will
 * not match any of them.
 *
 * Set this to the BU id you want the Launchpad to impersonate (e.g.
 * 11) and the BU main-file init will pass it into BU_Fw_init() in
 * place of the DIP switch value. Set to 0 to fall back to the
 * DIP switch reading (production hardware).
 *
 * Does NOT affect the S-Board / fw_bu_master side â€” that code only
 * reads IDs from CAN command payloads, it never cares what the
 * local board id is. */
#define FW_BU_BOARD_ID_OVERRIDE     0U

/* â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�
 *  M-BOARD â†’ S-BOARD COMMAND OPCODES
 *
 *  Sent on CAN ID 3 (existing M-Board command channel) in byte 0.
 *  Disjoint from the existing opcodes in common.h (CMD_START_DISCOVERY
 *  = 0x05, CMD_ERASE_BANK4 = 0x09, CMD_WRITE_DEFAULT = 0x0A, etc.).
 * â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•�â•� */

#define CMD_START_BU_FW_UPGRADE     0x0EU
    /* Payload:
     *   byte 1: target BU board id (11..22, or 0xFF = "all sequentially")
     *   byte 2: options bitmap
     *              bit 0 = continue_on_failure (1 = keep going to next BU)
     *              bit 1..7 = reserved
     */

#define CMD_BU_FW_STATUS_REQUEST    0x0FU
    /* No payload. S-Board replies on existing response path with:
     *   byte 0 = CMD_BU_FW_STATUS_REQUEST echo
     *   byte 1 = current target BU board id (or 0xFF if idle)
     *   byte 2 = master state (FW_BuMasterState enum value)
     *   byte 3 = master error code (FW_BuMasterError enum value)
     *   bytes 4..7  = bytes_sent (u32 LE)
     *   bytes 8..11 = image_size (u32 LE)
     *   byte 12 = last_completed_bu_id (bulk-update progress)
     *   bytes 13..14 = bitmap bits for which BUs have been successfully
     *                  upgraded this cycle (ID 11 = bit 0, ID 22 = bit 11)
     */

#endif /* FW_UPGRADE_CONFIG_H */
