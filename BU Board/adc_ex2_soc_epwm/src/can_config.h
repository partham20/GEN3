/**
 * @file can_config.h
 * @brief Configurable CAN bus mapping for BU Board
 *
 * This header maps the logical CAN bus to a physical MCAN instance.
 * The BU Board has a single CAN bus connecting to the S-Board.
 * To use a different MCAN module or GPIO pins, change ONLY these
 * definitions — all application code uses these names.
 *
 * Current mapping (production BU Board):
 *   MCANA (GPIO 1 TX, GPIO 0 RX) --> S-Board bus
 */

#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

/* ═══════════════════════════════════════════════════════════════
 *  S-Board bus  (BU-Board <-> S-Board)
 *
 *  Uses the same CAN_BU_* naming as the S-Board project so that
 *  shared headers (fw_upgrade_config.h) work in both projects.
 * ═══════════════════════════════════════════════════════════════ */
#define CAN_BU_BASE             MCANA_DRIVER_BASE
#define CAN_BU_MSG_RAM_BASE     MCANA_MSG_RAM_BASE
#define CAN_BU_TX_PIN           GPIO_1_MCANA_TX
#define CAN_BU_RX_PIN           GPIO_0_MCANA_RX
#define CAN_BU_SYSCTL           SYSCTL_MCANA
#define CAN_BU_INT_LINE1        INT_MCANA_1

#endif /* CAN_CONFIG_H */
