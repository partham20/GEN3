/**
 * @file common.h
 * @brief Common definitions and macros used across the project
 */

#ifndef COMMON_H
#define COMMON_H

#include "driverlib.h"
#include "device.h"
#include "inc/stw_types.h"
#include "inc/stw_dataTypes.h"
#include <string.h>
#include "can_config.h"

// CAN Message RAM Configuration
#define NUM_OF_MSG                      (32U)

#define MCAN_STD_ID_FILTER_NUM          (35U)
#define MCAN_EXT_ID_FILTER_NUM          (0U)

#define MCAN_FIFO_0_NUM                 (15U)
#define MCAN_FIFO_0_ELEM_SIZE           (MCAN_ELEM_SIZE_64BYTES)

#define MCAN_FIFO_1_NUM                 (0U)
#define MCAN_FIFO_1_ELEM_SIZE           (MCAN_ELEM_SIZE_64BYTES)

#define MCAN_RX_BUFF_NUM                (19U)
#define MCAN_RX_BUFF_ELEM_SIZE          (MCAN_ELEM_SIZE_64BYTES)

#define MCAN_TX_BUFF_SIZE               (32U)
#define MCAN_TX_FQ_SIZE                 (0U)
#define MCAN_TX_BUFF_ELEM_SIZE          (MCAN_ELEM_SIZE_64BYTES)
#define MCAN_TX_EVENT_SIZE              (32U)

// Message RAM Starting Addresses
#define MCAN_STD_ID_FILT_START_ADDR     (0x0U)
#define MCAN_EXT_ID_FILT_START_ADDR     (MCAN_STD_ID_FILT_START_ADDR + ((MCAN_STD_ID_FILTER_NUM * MCANSS_STD_ID_FILTER_SIZE_WORDS * 4U)))
#define MCAN_FIFO_0_START_ADDR          (MCAN_EXT_ID_FILT_START_ADDR + ((MCAN_EXT_ID_FILTER_NUM * MCANSS_EXT_ID_FILTER_SIZE_WORDS * 4U)))
#define MCAN_FIFO_1_START_ADDR          (MCAN_FIFO_0_START_ADDR + (MCAN_getMsgObjSize(MCAN_FIFO_0_ELEM_SIZE) * 4U * MCAN_FIFO_0_NUM))
#define MCAN_RX_BUFF_START_ADDR         (MCAN_FIFO_1_START_ADDR + (MCAN_getMsgObjSize(MCAN_FIFO_1_ELEM_SIZE) * 4U * MCAN_FIFO_1_NUM))
#define MCAN_TX_BUFF_START_ADDR         (MCAN_RX_BUFF_START_ADDR + (MCAN_getMsgObjSize(MCAN_RX_BUFF_ELEM_SIZE) * 4U * MCAN_RX_BUFF_NUM))
#define MCAN_TX_EVENT_START_ADDR        (MCAN_TX_BUFF_START_ADDR + (MCAN_getMsgObjSize(MCAN_TX_BUFF_ELEM_SIZE) * 4U * (MCAN_TX_BUFF_SIZE + MCAN_TX_FQ_SIZE)))

// Flash Memory Configuration
#define SECTOR1                         0x100000U // Sector 1 bank 4 start address
#define BANK4_START                     0x100000U
#define BANK4_END                       0x108000U // Bank 4: 0x100000–0x107FFF (32 sectors)
#define SECTOR_SIZE                     0x400U
#define WORDS_IN_FLASH_BUFFER           0x400U
#define WRAP_LIMIT                      0xFFFE
#define FLASH_SIZE                      FlashBank4EndAddress

// Flash Sector Write Enable Codes
#define SEC0TO31                        0x00000000U
#define SEC32To127                      0xFFFFFFFFU

// Buffer Sizes
#define TOTAL_BUFFER_SIZE               1024
#define calibrationdDataSize            451     // PDUData size in 16-bit words

// Command Codes
#define CMD_START_DISCOVERY             0x05
#define CMD_VERSION_CHECK               0x01
#define CMD_FLASH_ENABLE                0x02
#define CMD_READ                        0x03
#define CMD_STOP_CALIBRATION            0x06
#define CMD_ERASE_BANK4                 0x09
#define CMD_WRITE_DEFAULT               0x0A

// Status Codes
#define STATUS_FAIL                     0x00
#define STATUS_OK                       0x01

// BU Board Configuration
#define MAX_BU_BOARDS                   12      // Maximum BU boards
#define FRAMES_PER_BOARD                2
#define CTS_PER_BOARD                   18
#define FIRST_BU_ID                     11

// Frame Types
#define BU_RESPONSE_FRAME_TYPE          0x01    // Current gains
#define BU_KW_FRAME_TYPE                0x02    // kW gains

// Discovery Commands
#define DISCOVERY_START_CMD             0x0B
#define DISCOVERY_STOP_CMD              0x0C
#define DISCOVERY_RESPONSE_ACK          0x0D

/* ══════════════════════════════════════════════════════════════
 *  FW Update Mode — system-wide quiesce/resume control
 *  M-Board <-> S-Board on MCANB (CMD ID 7, RESP ID 8)
 *  S-Board <-> BU-Boards on MCANA (broadcast ID 4, reply 10+dip)
 * ══════════════════════════════════════════════════════════════ */
#define CMD_ENTER_FW_MODE               0x3C    /* M->S: stop everything, prepare for OTA */
#define CMD_EXIT_FW_MODE                0x3D    /* M->S: resume normal application */
#define CMD_FW_STATUS_REQ               0x3E    /* M->S: request aggregate FW status */
#define RESP_FW_SUMMARY                 0x40    /* S->M: aggregate summary payload */
#define RESP_FW_RESULT                  0x41    /* BU->S: per-board result (push+pull) */
#define RESP_BU_HEALTH                  0x42    /* S->M: periodic BU-fleet health bitmap
                                                 * (version mismatch / duplicate / dead) */

/* CAN-bus arbitration ID the S-Board uses for ALL frames it sends to the
 * M-Board on MCANB. Must match RESP_ID in m_board scripts/ota.sh (008).
 * The opcode (RESP_FW_SUMMARY / RESP_FW_RESULT / ...) lives in data[0]. */
#define FW_MODE_RESP_CAN_ID             8U

#define BU_CMD_ENTER_FW_MODE            0x1E    /* S->BU (broadcast MCANA id 4) */
#define BU_CMD_EXIT_FW_MODE             0x1F    /* S->BU */
#define BU_CMD_FW_STATUS_REQ            0x42    /* S->BU */
#define BU_FW_BROADCAST_CAN_ID          4U      /* MCANA broadcast to all BU boards */

/* Heartbeat in FW mode — 1 Hz, data[0]=board_id, data[1]=1 */
#define HEARTBEAT_FW_MODE_CAN_ID        0x6FE

/* FW-result status codes (data[1] of RESP_FW_RESULT) */
#define FW_RESULT_BOOTED                0x01    /* came up after reboot, alive */
#define FW_RESULT_NORMAL                0x02    /* running normal app, no update happened */
#define FW_RESULT_FAILED                0x03    /* boot manager reported failure */

/* Build-time firmware version. Local copy must be kept BYTE-FOR-BYTE
 * in sync with BU Board/adc_ex2_soc_epwm/firmware_version.h. */
#include "firmware_version.h"

/* App image region for CRC32 reporting -- MUST match the boot manager's
 * BANK0_APP_START / APP_SECTOR_COUNT.  The S-Board boot manager copies
 * Bank 2 -> Bank 0 starting at 0x082000 (sector 8) for up to 120 sectors
 * of 1 KB each = 120 KB.  Linker BEGIN is at 0x082000 too. */
#define APP_IMAGE_START                 0x082000UL
#define APP_IMAGE_MAX_SIZE              0x1E000UL   /* 120KB, Bank 0 sectors 8..127 */

// Retry Configuration
#define MAX_RETRIES                     4
#define MAX_RETRIES_CMD                 0x05

// Calibration Version
#define majorVersionNumber              1
#define minorVersionNumber              0

// Number of frames and CTs
#define NUM_FRAMES                      15
#define TOTAL_FRAMES                    15
#define numberOfCTs                     216  // Number of current and kW gains

// Runtime activity control
extern volatile bool runtimeActive;

// Function prototypes for ISRs
__interrupt void MCANIntr1ISR(void);
__interrupt void frameTrackingTimerISR(void);
__interrupt void timer10SecISR(void);
__interrupt void buDataCollectionTimerISR(void);
__interrupt void retryTimerISR(void);

// Common data types and structures forward declarations
typedef struct PDUData PDUData;
typedef struct CalibrationData CalibrationData;
typedef struct PDUDataManager PDUDataManager;
typedef struct CANFrame CANFrame;

// Boolean definitions if not already defined
#ifndef bool
typedef enum { false = 0, true = 1 } bool;
#endif

#endif // COMMON_H
