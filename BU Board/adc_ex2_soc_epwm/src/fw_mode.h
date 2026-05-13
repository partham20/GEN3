/*
 * fw_mode.h — BU-Board FW Update Mode control
 *
 * Mirror of the S-Board's fw_mode.  On receipt of BU_CMD_ENTER_FW_MODE
 * on MCANA (CAN ID 4) from the S-Board, the BU board suspends its
 * normal ADC / CT / power / branch-TX work.  BU_CMD_EXIT_FW_MODE
 * resumes it.
 *
 * A 1 Hz heartbeat (CAN ID 0x6FE) on MCANA keeps the S-Board informed.
 * On every boot the BU broadcasts its RESP_FW_RESULT (0x41) once with
 * its version + Bank 0 CRC32 so the S-Board can aggregate status.
 */

#ifndef SRC_FW_MODE_H_
#define SRC_FW_MODE_H_

#include <stdint.h>

/* ── Shared protocol constants (mirror of S-Board common.h) ───── */
#define BU_CMD_ENTER_FW_MODE            0x1E
#define BU_CMD_EXIT_FW_MODE             0x1F
#define BU_CMD_FW_STATUS_REQ            0x42
#define RESP_FW_RESULT                  0x41

#define HEARTBEAT_FW_MODE_CAN_ID        0x6FE

#define FW_RESULT_BOOTED                0x01
#define FW_RESULT_NORMAL                0x02
#define FW_RESULT_FAILED                0x03

/* Build-time firmware version. Local copy must be kept BYTE-FOR-BYTE
 * in sync with S board/adc_ex2_soc_epwm/firmware_version.h. */
#include "../firmware_version.h"

/* App image region for CRC32 reporting -- MUST match BU bu_boot_manager's
 * BANK0_APP_START / APP_SECTOR_COUNT.  The boot manager copies Bank 2 ->
 * Bank 0 starting at 0x082000 (sector 8) for up to 120 sectors of 1 KB
 * each = 120 KB.  Linker BEGIN is at 0x082000 too. */
#define APP_IMAGE_START                 0x082000UL
#define APP_IMAGE_MAX_SIZE              0x1E000UL   /* 120KB, sectors 8..127 */

/* Boot flag sector (Bank 3 sector 0) — must match BU bu_boot_manager.
 * After a successful OTA copy, the boot manager re-programs this
 * sector with FW_FLAG_INSTALLED in word 0 and the installed image
 * size (8-bit-byte units) in words 2..3, so the freshly booted app
 * can CRC only the actually-installed bytes -- not the whole
 * APP_IMAGE_MAX_SIZE region with trailing 0xFFFF padding. */
#define FW_BANK3_FLAG_ADDR              0x0E0000UL
#define FW_FLAG_INSTALLED               0x9696U

typedef uint16_t uint8_t;
typedef int16_t int8_t;


typedef enum {
    FW_MODE_NORMAL = 0,
    FW_MODE_ACTIVE
} FwModeState;

/* ── Public API ──────────────────────────────────────────────── */
void FwMode_init(void);
void FwMode_tick(void);
FwModeState FwMode_getState(void);
static inline uint8_t FwMode_isActive(void) {
    return (uint8_t)(FwMode_getState() == FW_MODE_ACTIVE);
}

/* ISR-safe: returns 1 if data[0] is a fw-mode command we handled. */
uint8_t FwMode_isrOnBroadcast(const uint8_t *data);

/* Call once from startup after CAN is up. Broadcasts boot status. */
void FwMode_sendBootStatus(void);

#endif /* SRC_FW_MODE_H_ */
