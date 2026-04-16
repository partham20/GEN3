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

// CRC Configuration
#define CRC16_POLYNOMIAL                0x8005 // CRC16 polynomial

// Buffer Sizes
#define TOTAL_BUFFER_SIZE               1024
#define calibrationdDataSize            451

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
