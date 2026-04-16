/**
 * @file can_config.h
 * @brief Configurable CAN bus mapping for S-Board
 *
 * This header maps logical CAN buses to physical MCAN instances.
 * To swap which MCAN module connects to which bus, change ONLY
 * the definitions below — all application code uses these names.
 *
 * Current mapping (production S-Board):
 *   MCANA (GPIO 1 TX, GPIO 0 RX) --> BU-Board bus
 *   MCANB (GPIO 2 TX, GPIO 3 RX) --> M-Board bus
 */

#ifndef CAN_CONFIG_H
#define CAN_CONFIG_H

/* ═══════════════════════════════════════════════════════════════
 *  BU-Board bus  (S-Board <-> BU-Boards)
 * ═══════════════════════════════════════════════════════════════ */
#define CAN_BU_BASE             MCANA_DRIVER_BASE
#define CAN_BU_MSG_RAM_BASE     MCANA_MSG_RAM_BASE
#define CAN_BU_TX_PIN           GPIO_1_MCANA_TX
#define CAN_BU_RX_PIN           GPIO_0_MCANA_RX
#define CAN_BU_SYSCTL           SYSCTL_MCANA
#define CAN_BU_INT_LINE1        INT_MCANA_1

/* ═══════════════════════════════════════════════════════════════
 *  M-Board bus  (S-Board <-> M-Board)
 * ═══════════════════════════════════════════════════════════════ */
#define CAN_MBOARD_BASE         MCANB_DRIVER_BASE
#define CAN_MBOARD_MSG_RAM_BASE MCANB_MSG_RAM_BASE
#define CAN_MBOARD_TX_PIN       GPIO_2_MCANB_TX
#define CAN_MBOARD_RX_PIN       GPIO_3_MCANB_RX
#define CAN_MBOARD_SYSCTL       SYSCTL_MCANB
#define CAN_MBOARD_INT_LINE1    INT_MCANB_1

#endif /* CAN_CONFIG_H */
