# BU Firmware Upgrade — S-Board ↔ BU-Board

New code "branch" for OTA-flashing BU boards from the S-Board.

This sits **alongside** the existing `Firmware Upgrade/` folder (which
handles the S-Board's *own* self-update into Bank 2). It does not modify
or replace any of those files.

## Files added

### S-Board (`adc_ex2_soc_epwm/BU Firmware Upgrade/`)

| File | Role |
|---|---|
| `fw_bu_image_rx.h/c` | Receives a BU `.bin` from the M-Board over CAN-FD and stores it in **Bank 1** (`0x0A0000`). Bank 1 is reserved as BU staging — never copied into S-Board's own Bank 0. |
| `fw_bu_master.h/c`   | Streams the staged Bank 1 image to one BU board over MCANA. State machine: `PREPARE → HEADER → DATA bursts → VERIFY → ACTIVATE → BOOT_OK`. |

### BU-Board (`adc_ex2_soc_epwm/src/Firmware Upgrade/`)

| File | Role |
|---|---|
| `fw_update_bu.h/c` | Receives FW commands and data from the S-Board over MCANA, writes incoming bytes into **Bank 2** (`0x0C0000`), verifies CRC, writes boot flag into Bank 3, resets. The boot manager (loaded by JTAG) finishes the swap on next boot. |

## CAN ID map

| ID | Direction | Use |
|----|-----------|-----|
| `0x18` | M-Board → S-Board | BU FW data frames |
| `0x19` | M-Board → S-Board | BU FW commands |
| `0x1A` | S-Board → M-Board | BU FW responses |
| `0x30` | S-Board → BU       | FW data frames |
| `0x31` | S-Board → BU       | FW commands |
| `0x32` | BU → S-Board       | FW responses |

These IDs were chosen to **not** collide with anything in use today:
- `1..5` operational CAN traffic
- `6..8` existing self-update receiver in `Firmware Upgrade/fw_image_rx.c`
- `11..28` BU board IDs

## Top-level data flow

```
M-Board                S-Board                              BU-Board
   │                      │                                    │
   │── 0x18/0x19 ────────►│   FW_BuImageRx_process()           │
   │   (bin + commands)   │   writes Bank 1 (staging)          │
   │                      │                                    │
   │   FW_BuMaster_start(boardId)                              │
   │                      │                                    │
   │                      │── 0x31 PREPARE ──────────────────►│  erase Bank 2
   │                      │◄── 0x32 ACK ──────────────────────│
   │                      │── 0x31 HEADER (size,crc,ver) ────►│
   │                      │◄── 0x32 ACK ──────────────────────│
   │                      │── 0x30 DATA × 16 (burst) ───────►│  ← write Bank 2
   │                      │◄── 0x32 ACK (last seq) ───────────│
   │                      │   ... repeat ...                  │
   │                      │── 0x31 VERIFY ───────────────────►│  CRC32 check
   │                      │◄── 0x32 VERIFY_PASS ──────────────│
   │                      │── 0x31 ACTIVATE ─────────────────►│  write boot flag, reset
   │                      │   (BU reboots, boot manager       │
   │                      │    copies Bank 2 → Bank 0)        │
   │                      │◄── 0x32 BOOT_OK (version) ────────│
```

## Wiring it into the S-Board build

1. Add `BU Firmware Upgrade/` to the CCS project file list (right-click
   project → *Add Files* → both `.c` files).
2. In `adc_ex2_soc_epwm.c`, after `MCANAConfig()` and the existing
   `FW_ImageRx_init()` call, add:
   ```c
   #include "BU Firmware Upgrade/fw_bu_image_rx.h"
   #include "BU Firmware Upgrade/fw_bu_master.h"

   FW_BuImageRx_init();
   FW_BuMaster_init();
   ```
3. In the main loop (next to `FW_ImageRx_process()`):
   ```c
   FW_BuImageRx_process();
   FW_BuMaster_process();
   ```
4. In `isr.c` MCAN ISR, add two new branches alongside the existing
   `FW_DATA_CAN_ID` / `FW_CMD_CAN_ID` branches:
   ```c
   else if (boardId == FW_BU_DATA_CAN_ID)   /* 0x18 */
       FW_BuImageRx_isrOnData(&rxMsg[i]);
   else if (boardId == FW_BU_CMD_CAN_ID)    /* 0x19 */
       FW_BuImageRx_isrOnCommand(&rxMsg[i]);
   else if (boardId == FW_BU_TX_RESP_CAN_ID) /* 0x32 */
       FW_BuMaster_isrOnResponse(&rxMsg[i]);
   ```
5. In `can_driver.c` `MCANAConfig()`, extend the standard ID filter
   table to route IDs `0x18`, `0x19`, `0x32` into dedicated RX
   buffers (or the existing FIFO0 — the ISR already dispatches by
   `boardId`).
6. `fw_bu_master.c` references `g_systemTickMs` for coarse phase
   timeouts. If a millisecond tick variable does not yet exist in
   `timer_driver.c`, add one driven by an existing periodic ISR (or
   substitute a CPU Timer 2 free-running counter divided to ms).

## Wiring it into the BU-Board build

1. Add `src/Firmware Upgrade/fw_update_bu.c` to the BU CCS project.
2. Update `src/can_bu.c` `MCANConfig()` filter table to add two more
   entries:
   ```c
   /* Filter 4: ID 0x30 (FW data) -> RX FIFO 0 */
   stdFiltelem.sfid1 = 0x030U; stdFiltelem.sfid2 = 0x030U;
   stdFiltelem.sfec  = MCAN_STDFILTEC_FIFO0;
   MCAN_addStdMsgIDFilter(MCANA_DRIVER_BASE, 4U, &stdFiltelem);

   /* Filter 5: ID 0x31 (FW cmd) -> RX FIFO 0 */
   stdFiltelem.sfid1 = 0x031U; stdFiltelem.sfid2 = 0x031U;
   stdFiltelem.sfec  = MCAN_STDFILTEC_FIFO0;
   MCAN_addStdMsgIDFilter(MCANA_DRIVER_BASE, 5U, &stdFiltelem);
   ```
   …and bump `MCAN_STD_ID_FILTER_NUM` from 4 to 6 in `can_bu.h`.
3. In `can_isr.c`, after the existing `VoltageRx_interruptHandler`
   call, dispatch FW frames:
   ```c
   /* Pull frame ID out of element header and route */
   uint32_t rxId = (g_rxMsg[bufIdx].id >> 18U) & 0x7FFU;
   if (rxId == BU_FW_DATA_CAN_ID || rxId == BU_FW_CMD_CAN_ID)
       BU_Fw_isrOnFrame(rxId, &g_rxMsg[bufIdx]);
   ```
   (For frames coming through FIFO 0 use the same dispatch.)
4. In `adc_ex2_soc_epwm.c` (BU main):
   ```c
   #include "src/Firmware Upgrade/fw_update_bu.h"
   ...
   BU_Fw_init(address);   /* address = DIP-switch board id */
   ...
   while (1)
   {
       if (BU_Fw_isBusy())
       {
           /* Suspend ADC / power-calc work — flash writes can't
              be safely interleaved with the 5.12 kHz sample loop. */
           BU_Fw_process();
           continue;
       }
       /* normal operation */
       ...
       BU_Fw_process();
   }
   ```
5. **Flash library**: `fw_update_bu.c` defaults to `BU_FW_FLASH_STUB=1`
   — the CAN protocol is exercised but flash writes are no-ops. To
   enable real flash writes:
   - Copy `FAPI_F28P55x_EABI_v4.00.00.lib` from the S-Board project
     into the BU project root.
   - Copy `flash_programming_f28p55x.h` and `FlashTech_F28P55x_C28x.h`
     from C2000Ware into the BU project (or set the include path).
   - Add `.TI.ramfunc` allocation to `28p55x_generic_flash_lnk.cmd`
     (mirror what the S-Board linker file does).
   - Compile with `-DBU_FW_FLASH_STUB=0`.
6. Bring up the BU boot manager (separate CCS project, identical to
   the S-Board boot_manager) and flash it into Bank 0 sectors 0..3
   via JTAG before testing the activate path.

## Test checklist

- [ ] M-Board (or `fw_sender.py` over USB-CAN) sends BU `.bin` to S-Board
      → S-Board `FW_BuImageRx_getState()` reaches `FW_BU_RX_READY`.
- [ ] CCS watch expression: dump `0x0A0000` and verify it matches the
      first bytes of the `.bin` file.
- [ ] `FW_BuMaster_start(11)` → `FW_BuMaster_getState()` walks
      `PREPARING → SEND_HEADER → SENDING_DATA → VERIFYING → ACTIVATING → DONE`.
- [ ] On the BU side, watch `BU_Fw_getState()` walk
      `PREPARING → WAITING_HEADER → RECEIVING → VERIFYING → ACTIVATING`.
- [ ] After ACTIVATE the BU board resets and the new firmware boots —
      verify by dumping its first bytes and checking the version
      reported in the `BOOT_OK` response.
- [ ] Pull power mid-stream → BU should come back up on the **old**
      firmware (boot flag never written, boot manager skips the copy).
