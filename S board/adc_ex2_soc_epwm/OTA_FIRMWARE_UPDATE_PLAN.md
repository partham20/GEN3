# OTA Firmware Update — S-Board & BU-Board
### GEN3 Distributed Power Monitoring System — Data Center PDU
**Document Date**: 2026-04-02 | **Author**: Parthasarathy M | **Hardware**: TMS320F28P55x (C2000)

---

## 1. System Architecture

```
M-Board (Management Controller)
    │ MCANB — CAN ID 3 (firmware images, commands)
    ▼
S-Board (Master — TMS320F28P550SG9, 128-pin)
    │ MCANA — CAN-FD (firmware data to BU boards)
    ├── BU-Board #1  (TMS320F28P550SJ9, 64-pin)
    ├── BU-Board #2
    └── ... up to 18 BU-Boards
```

**Update flow summary:**
1. M-Board sends both firmware images to S-Board over MCANB
2. S-Board stores BU firmware in Bank 1, S-Board firmware in Bank 2
3. S-Board flashes all 18 BU-Boards sequentially over MCANA
4. S-Board updates itself last

**Total update time: ~62 seconds for all 19 boards**

---

## 2. Architecture Decision — Two-Stage Boot (Brick-Proof)

This system is deployed in **data center PDUs** — physical access after rack installation is not guaranteed. A bricked board means loss of power monitoring for an entire rack.

### Why Two-Stage Boot Is Required

A single-stage OTA (running app copies Bank 2 → Bank 0 then resets) has a **1.5-second danger window** where power loss bricks the board permanently. This is unacceptable for critical infrastructure.

### Solution: Boot Manager + Boot Flag

```
Power on
    │
    ▼
ROM Bootloader → jumps to 0x080000 (always)
    │
    ▼
┌─────────────────────────────────────────────┐
│  Boot Manager (Bank 0, Sectors 0–3, ~4KB)   │
│  NEVER erased. NEVER updated via OTA.        │
│                                             │
│  1. Read Bank 3 boot flag                   │
│  2. flag == 0xA5A5 AND CRC valid?           │
│     YES → Copy Bank 2 → Bank 0 (Sec 4-127) │
│            Clear flag → Reset               │
│     NO  → Jump to 0x081000 (current app)   │
└─────────────────────────────────────────────┘
```

### Brick-Proof Scenarios

| Scenario | What Happens |
|---|---|
| Power loss before boot flag written | Boot manager → Bank 0 app (old firmware, safe) |
| Power loss during boot flag write | Flag invalid (partial write) → Bank 0 app (safe) |
| Power loss during boot manager copy | Flag still set → boot manager retries on next boot |
| Bank 2 CRC mismatch in boot manager | Flag cleared → Bank 0 app (safe) |
| New firmware crashes immediately | Watchdog reset → boot manager sees cleared flag → Bank 0 app |
| Corrupt Bank 2 | Boot manager CRC fails → Bank 0 app (safe) |

**No brick scenario exists.**

---

## 3. Flash Memory Layout

### S-Board Flash Map

```
Bank 0: 0x080000 – 0x09FFFF  (128KB)
  Sector 0–3    0x080000 – 0x080FFF   Boot Manager        ← NEVER ERASED
  Sector 4–127  0x081000 – 0x09FFFF   S-Board Application (erasable)

Bank 1: 0x0A0000 – 0x0BFFFF  (128KB)   BU Firmware Image (raw binary, ~38KB)
Bank 2: 0x0C0000 – 0x0DFFFF  (128KB)   New S-Board Firmware Staging (raw binary, ~82KB)
Bank 3: 0x0E0000 – 0x0FFFFF  (128KB)   Image Metadata + Boot Flag
Bank 4: 0x100000 – 0x107FFF   (32KB)   Calibration Data (unchanged, existing)
```

### BU-Board Flash Map

```
Bank 0: 0x080000 – 0x09FFFF  (128KB)
  Sector 0–3    0x080000 – 0x080FFF   Boot Manager        ← NEVER ERASED
  Sector 4–127  0x081000 – 0x09FFFF   BU Application (erasable)

Bank 2: 0x0C0000 – 0x0DFFFF  (128KB)   New BU Firmware Staging (raw binary, ~38KB)
Bank 4: 0x100000 – 0x107FFF   (32KB)   Calibration Data (unchanged)
```

### Storage Budget

```
BU firmware:      ~38KB raw  → Bank 1 (128KB)  → 70% spare
S-Board firmware: ~82KB raw  → Bank 2 (128KB)  → 36% spare
Metadata:         ~64 bytes  → Bank 3 (128KB)  → 99.9% spare
```

### Bank 3 — Boot Flag Structure

```c
typedef struct {
    uint16_t updatePending;   // 0xA5A5 = update verified and ready to apply
    uint16_t crcValid;        // 0x5A5A = Bank 2 CRC confirmed by app
    uint32_t imageSize;       // Image size in 16-bit words
    uint32_t imageCRC;        // CRC32 of image (boot manager re-verifies)
    uint32_t version;         // Firmware version number
} BootFlag;
```

### Bank 3 — Image Metadata Structure

```c
typedef struct {
    uint16_t magic;           // 0xFE01 = valid structure
    uint16_t imageType;       // 0 = BU firmware, 1 = S-Board firmware
    uint32_t imageSize;       // Size in 16-bit words
    uint32_t imageCRC;        // CRC32 of image data
    uint32_t version;         // Firmware version
    uint32_t destBank;        // Target bank start address
    uint32_t entryPoint;      // Application entry (0x081000)
    uint16_t valid;           // 0xA5A5 = received and CRC verified
    uint16_t activated;       // 0x5A5A = successfully applied
} FW_ImageHeader;
```

---

## 4. CAN Protocol

### Frame Format (CAN-FD, 64 bytes)

```
Byte 0:      Command code
Byte 1:      Target board ID (0xFF = broadcast)
Bytes 2–3:   Sequence number (for data frames)
Bytes 4–63:  Payload (60 bytes of firmware data per data frame)
```

### Command Codes

```
M-Board → S-Board (MCANB, CAN ID 3)
  0x30  CMD_FW_TRANSFER_START    Begin transfer session
  0x31  CMD_FW_IMAGE_HEADER      Image metadata (type, size, CRC, version)
  0x32  CMD_FW_IMAGE_DATA        Firmware data (60 bytes payload per frame)
  0x33  CMD_FW_IMAGE_COMPLETE    End of one image
  0x34  CMD_FW_TRANSFER_DONE     All images sent
  0x35  CMD_FW_START_BU_UPDATE   Trigger BU flashing sequence
  0x36  CMD_FW_START_SELF_UPD    Trigger S-Board self-update
  0x37  CMD_FW_STATUS_REQUEST    Poll update progress

S-Board → BU-Board (MCANA, CAN ID 4 broadcast)
  0x20  CMD_FW_UPDATE_PREPARE    Prepare BU for update (stop normal ops, erase staging)
  0x21  CMD_FW_DATA              Firmware data (60 bytes payload)
  0x22  CMD_FW_VERIFY            Request CRC verification of staging bank
  0x23  CMD_FW_ACTIVATE          Write boot flag and reset
  0x24  CMD_FW_ABORT             Abort, resume normal operation

BU-Board → S-Board (MCANA, BU CAN ID 11–28)
  0x25  CMD_FW_ACK               Acknowledge (includes last seq# received)
  0x26  CMD_FW_NAK               Error (includes error code)
  0x27  CMD_FW_VERIFY_PASS       CRC match confirmed
  0x28  CMD_FW_VERIFY_FAIL       CRC mismatch
  0x29  CMD_FW_BOOT_OK           New firmware booted successfully (includes version)
```

### Flow Control — 16-Frame Burst

- Sender transmits burst of **16 frames** (16 × 60 = 960 bytes)
- Receiver ACKs with sequence number of last frame received
- Sender waits up to **500ms** for ACK
- Timeout → retransmit entire burst (up to **3 retries**)
- 3 failures → mark board FAILED, continue to next board
- Failed board is NOT bricked — still running old firmware

---

## 5. Firmware Image Format

**Raw binary (.bin)** — not Intel Hex.

- Generated by CCS Hex Utility (`hex2000`)
- Linked to run from `0x081000` (app entry point, past boot manager)
- CRC32 computed on host (PC/M-Board) over raw binary before sending

### Generating the Binary in CCS

1. Right-click project → **Properties → Build → C2000 Hex Utility**
2. Enable: **Generate hex file**
3. Output format: **Binary**
4. Build → `.bin` file produced alongside `.out`

### Computing CRC32 on Host (Python)

```python
import struct

def crc32(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xEDB88320
            else:
                crc >>= 1
    return crc ^ 0xFFFFFFFF

with open("sboard_firmware.bin", "rb") as f:
    data = f.read()

print(f"Image size : {len(data)} bytes ({len(data)//2} words)")
print(f"CRC32      : 0x{crc32(data):08X}")
```

Send size and CRC in `CMD_FW_IMAGE_HEADER` frame before data transfer begins.

---

## 6. Software Projects — Build Order

### Project 0 — Boot Manager (New, Separate CCS Project)

**Target**: Both S-Board and BU-Board  
**Flash location**: Bank 0, Sectors 0–3 (0x080000–0x080FFF)  
**Loaded via**: JTAG only — never updated via OTA

**Logic:**
```c
// Runs from 0x080000, linked entirely within sectors 0-3
void main(void)
{
    BootFlag *flag = (BootFlag *)BANK3_FLAG_ADDR;

    if (flag->updatePending == 0xA5A5 && flag->crcValid == 0x5A5A)
    {
        uint32_t computedCRC = FW_calculateCRC(BANK2_START, flag->imageSize);
        if (computedCRC == flag->imageCRC)
        {
            // Safe to copy
            FW_bankSwap(BANK2_START, BANK0_APP_START, flag->imageSize);
            // FW_bankSwap clears flag, copies, resets — never returns
        }
        // CRC failed: clear flag, fall through to current app
        flag->updatePending = 0x0000;
    }

    // Jump to application
    ((void (*)(void))0x081000)();
}
```

**Linker constraint**: entire boot manager must fit in 4 sectors (4KB). Keep it minimal — no peripherals, no CAN, flash reads only.

---

### Project 1 — Flash Writer Library (Common to Both Boards)

**Files**: `fw_update_flash.c`, `fw_update_flash.h`  
**Section**: `.TI.ramfunc` for all flash-touching functions

**Functions to implement:**

```c
// Initialize Flash API
void FW_Flash_init(void);

// Erase sectorCount sectors starting at bankBaseAddress
// Returns 0 = success, nonzero = error
uint16_t FW_eraseFlashRange(uint32_t bankBaseAddress, uint32_t sectorCount);

// Program one 512-bit block (8 x uint16 words) at destAddr
// destAddr must be 8-word aligned
uint16_t FW_programBlock(uint32_t destAddr, uint16_t *srcBuf);

// Compute CRC32 over a flash region (word-addressable)
uint32_t FW_calculateCRC(uint32_t startAddr, uint32_t numWords);

// Verify CRC32 of a flash region against expected value
// Returns 0 = match, 1 = mismatch
uint16_t FW_verifyCRC(uint32_t startAddr, uint32_t numWords, uint32_t expectedCRC);

// Write boot flag to Bank 3 and reset device — DOES NOT RETURN
// Called by app after CRC verification passes
void FW_triggerUpdate(uint32_t imageSize, uint32_t imageCRC, uint32_t version);

// RAM-resident: erase dstBankAddr sectors, copy srcBankAddr → dstBankAddr, reset
// Called ONLY by boot manager
// #pragma CODE_SECTION(FW_bankSwap, ".TI.ramfunc")
void FW_bankSwap(uint32_t srcBankAddr, uint32_t dstBankAddr, uint32_t numWords);
```

**Key rules:**
- All functions that touch flash registers must be in `.TI.ramfunc`
- Flash writes are **8 × uint16 words (512-bit)** per call — hardware requirement
- `EALLOW` before any flash operation, `EDIS` after
- Always `ClearFSMStatus()` before each erase or program command
- Verify FMSTAT == 3 after every operation
- `FW_bankSwap` is called ONLY by the boot manager — never by the application

**Test plan (via CCS watch expressions):**
1. Call `FW_eraseFlashRange(0x0C0000, 128)` → verify Bank 2 blank
2. Call `FW_programBlock(0x0C0000, testBuf)` → verify 8 words written
3. Call `FW_verifyCRC(0x0C0000, 64, expectedCRC)` → expect pass
4. Pre-load test binary in Bank 2, set boot flag manually in Bank 3 → reset → verify boot manager copies to Bank 0

---

### Project 2 — BU-Board Firmware Update Receiver

**Files**: `fw_update_bu.c`, `fw_update_bu.h`  
**Added to**: BU-Board firmware build

**State machine:**

```
FW_STATE_IDLE
    │  CMD_FW_UPDATE_PREPARE received
    ▼
FW_STATE_PREPARING   ← erase Bank 2 staging area
    │  ACK sent when erase complete
    ▼
FW_STATE_RECEIVING   ← write incoming CMD_FW_DATA frames to Bank 2
    │  ACK every 16th frame
    │  CMD_FW_VERIFY received
    ▼
FW_STATE_VERIFYING   ← compute CRC32 over written region
    │  VERIFY_PASS or VERIFY_FAIL sent
    │  CMD_FW_ACTIVATE received (only if VERIFY_PASS)
    ▼
FW_STATE_ACTIVATING  ← call FW_triggerUpdate() → writes boot flag → resets
```

**Important:** When in `FW_STATE_PREPARING` or `FW_STATE_RECEIVING`, stop normal ADC and power calculation operations. Flash writes are CPU-intensive and must not be interrupted.

**On CMD_FW_ABORT at any state:** Erase Bank 2, clear any partial data, return to `FW_STATE_IDLE`, resume normal operation.

---

### Project 3 — S-Board CAN Flash Master (Sender to BU Boards)

**Files**: `fw_master.c`, `fw_master.h`  
**Added to**: S-Board firmware build

**Flow for each BU board:**

```
S-Board                              BU-Board #N
   │                                      │
   │── CMD_FW_UPDATE_PREPARE (board_id) ──►  Stop ops, erase Bank 2
   │◄── CMD_FW_ACK (ready) ───────────────│  (timeout 5s)
   │                                      │
   │── CMD_FW_DATA (seq 0)  ──────────────►
   │── CMD_FW_DATA (seq 1)  ──────────────►
   │   ... (burst of 16 frames)
   │── CMD_FW_DATA (seq 15) ──────────────►  Write to Bank 2
   │◄── CMD_FW_ACK (seq 15) ──────────────│  (timeout 500ms, 3 retries)
   │
   │   ... repeat until all ~634 frames sent (~38KB)
   │
   │── CMD_FW_VERIFY (CRC32, size) ────────►  Compute CRC over Bank 2
   │◄── CMD_FW_VERIFY_PASS ───────────────│  (timeout 5s)
   │
   │── CMD_FW_ACTIVATE ─────────────────►  Write boot flag, reset
   │
   │   Wait up to 10s for reboot
   │◄── CMD_FW_BOOT_OK (version) ─────────│  New firmware booted
   │
   │   Log: "BU #N updated to version X"
```

**On any timeout or VERIFY_FAIL:** Send `CMD_FW_ABORT`. Mark board as failed. Move to next board. Do not stop the overall update.

---

### Project 4 — S-Board Image Receiver (from M-Board)

**Files**: `fw_image_rx.c`, `fw_image_rx.h`  
**Added to**: S-Board firmware build

**Flow:**

```
M-Board                              S-Board
   │                                      │
   │── CMD_FW_TRANSFER_START ─────────────►  Erase Banks 1, 2, 3
   │◄── ACK (ready) ──────────────────────│
   │                                      │
   │── CMD_FW_IMAGE_HEADER (BU, 38KB, CRC)►  Store metadata in Bank 3
   │◄── ACK ──────────────────────────────│
   │── CMD_FW_IMAGE_DATA × 634 frames ────►  Write to Bank 1 (60 bytes/frame)
   │── CMD_FW_IMAGE_COMPLETE ─────────────►  Verify CRC of Bank 1
   │◄── VERIFY_PASS ──────────────────────│
   │                                      │
   │── CMD_FW_IMAGE_HEADER (S-Board, 82KB)►  Store metadata in Bank 3
   │◄── ACK ──────────────────────────────│
   │── CMD_FW_IMAGE_DATA × 1367 frames ───►  Write to Bank 2 (60 bytes/frame)
   │── CMD_FW_IMAGE_COMPLETE ─────────────►  Verify CRC of Bank 2
   │◄── VERIFY_PASS ──────────────────────│
   │                                      │
   │── CMD_FW_TRANSFER_DONE ─────────────►  Mark both images valid in Bank 3
   │◄── ACK (both images OK) ─────────────│
```

---

### Project 5 — S-Board Self-Update

**Files**: `fw_self_update.c`, `fw_self_update.h`  
**Added to**: S-Board firmware build

**Flow (triggered by CMD_FW_START_SELF_UPD from M-Board):**

```c
uint16_t SelfUpdate_execute(void)
{
    // 1. Read metadata from Bank 3
    FW_ImageHeader *hdr = (FW_ImageHeader *)BANK3_SBOARD_HDR_ADDR;

    // 2. Check magic and valid flag
    if (hdr->magic != 0xFE01 || hdr->valid != 0xA5A5)
        return ERR_INVALID_HEADER;

    // 3. Re-verify Bank 2 CRC
    if (FW_verifyCRC(BANK2_START, hdr->imageSize, hdr->imageCRC) != 0)
        return ERR_CRC_MISMATCH;

    // 4. Notify M-Board — last transmission before silence
    CAN_send("SELF_UPDATE_STARTING");
    DELAY_US(10000);  // 10ms — let frame transmit

    // 5. Write boot flag and reset — DOES NOT RETURN
    FW_triggerUpdate(hdr->imageSize, hdr->imageCRC, hdr->version);

    return 0;  // never reached
}
```

Boot manager handles the actual copy on next boot. S-Board reboots in ~3 seconds with new firmware.

---

### Project 6 — Integration

**S-Board main loop additions:**
- New M-Board command handlers: `CMD_FW_TRANSFER_START` through `CMD_FW_START_SELF_UPD`
- Top-level state machine:

```
FW_IDLE
  → FW_RECEIVING    (CMD_FW_TRANSFER_START received from M-Board)
  → FW_BU_UPDATING  (CMD_FW_START_BU_UPDATE, after both images verified)
  → FW_SELF_UPDATING (CMD_FW_START_SELF_UPD, after all 18 BU boards done)
  → FW_DONE         (after reset and new firmware boot)
```

- Progress reporting: respond to `CMD_FW_STATUS_REQUEST` with current state, board number, frame count
- After successful boot: send version string to M-Board on first CAN heartbeat

**BU-Board main loop additions:**
- Check for firmware update commands in CAN RX handler
- When `FW_STATE_RECEIVING`: skip ADC processing, dedicate CPU to flash writes
- After successful boot: send `CMD_FW_BOOT_OK` with new version on startup

**Linker file changes (both boards):**
- Reserve `0x080000–0x080FFF` (sectors 0–3) for boot manager — mark as `RESERVE`
- Move application entry point to `0x081000`
- Both active app and staged app must be linked with entry at `0x081000`

---

## 7. Complete Update Sequence — End to End

```
TOTAL TIME: ~62 seconds

Phase 0 — Image Transfer (M-Board → S-Board): ~5 seconds
  1. M-Board sends CMD_FW_TRANSFER_START
  2. S-Board erases Banks 1, 2, 3
  3. M-Board sends BU image header + 634 data frames → Bank 1
  4. S-Board verifies Bank 1 CRC → PASS
  5. M-Board sends S-Board image header + 1367 data frames → Bank 2
  6. S-Board verifies Bank 2 CRC → PASS
  7. M-Board sends CMD_FW_TRANSFER_DONE
  8. S-Board marks both images valid in Bank 3 metadata

Phase 1 — BU-Board Updates (S-Board → 18 BU Boards): ~54 seconds
  Per board (~3 seconds each):
  1. S-Board sends CMD_FW_UPDATE_PREPARE to BU #N
  2. BU #N stops normal operation, erases Bank 2 staging
  3. S-Board streams BU firmware (634 frames × 60 bytes = 38KB)
  4. S-Board sends CMD_FW_VERIFY with CRC32 and size
  5. BU #N computes CRC over Bank 2 → sends VERIFY_PASS
  6. S-Board sends CMD_FW_ACTIVATE
  7. BU #N writes boot flag to Bank 3, resets
  8. Boot manager copies Bank 2 → Bank 0, clears flag, resets
  9. BU #N boots new firmware, sends CMD_FW_BOOT_OK
  10. S-Board logs: "BU #N updated to version X"
  Repeat for boards #11 through #28

Phase 2 — S-Board Self-Update: ~3 seconds
  1. S-Board reports "All 18 BU boards updated" to M-Board
  2. S-Board verifies Bank 3 metadata and Bank 2 CRC (final check)
  3. S-Board sends "SELF_UPDATE_STARTING" to M-Board
  4. S-Board calls FW_triggerUpdate() — writes boot flag, resets
  5. Boot manager copies Bank 2 → Bank 0, clears flag, resets
  6. S-Board boots new firmware
  7. S-Board re-discovers BU boards, resumes normal operation
  8. S-Board sends "Update complete, version X" to M-Board
```

---

## 8. Per-Board Timing Breakdown

| Phase | Time |
|---|---|
| Erase Bank 2 (128 sectors × 2KB) | ~0.5s |
| Transfer 38KB at 320 KB/s (with ACK overhead) | ~0.5s |
| CRC verify | ~0.1s |
| Boot manager copy + boot | ~1.5s |
| Boot confirm (CMD_FW_BOOT_OK) | ~0.4s |
| **Total per BU board** | **~3 seconds** |
| **Total for 18 BU boards** | **~54 seconds** |
| Phase 0 (image transfer to S-Board) | ~5 seconds |
| Phase 2 (S-Board self-update) | ~3 seconds |
| **Grand total** | **~62 seconds** |

---

## 9. Error Handling

| Error | Detection | Recovery |
|---|---|---|
| CAN frame lost | ACK timeout (500ms) | Retransmit burst (3 retries) |
| 3 retransmit failures | Retry counter exhausted | Send CMD_FW_ABORT, mark board failed, continue |
| Staging bank CRC mismatch | VERIFY_FAIL from BU | Retransmit entire image or mark failed |
| Boot flag write fails | Flash error status | Board stays on old firmware |
| Boot manager CRC fail | Mismatch on re-verify | Clear flag, boot old firmware |
| New firmware crashes | Watchdog timeout | Reset → boot manager → old firmware |
| S-Board self-update: pre-check fail | Bad magic / CRC | Return error, do NOT proceed, report to M-Board |

---

## 10. Risks and Mitigations

| Risk | Severity | Mitigation |
|---|---|---|
| Boot manager itself corrupted | Critical | Loaded via JTAG only. Write-protect sectors 0-3 via DCSM if available. |
| Flash API library (~40KB) added to BU | Low | BU Bank 0 has ~90KB spare — confirmed fits |
| 18 boards take too long | Low | ~54s acceptable for maintenance window. Can parallelize 2-3 boards if needed. |
| BU board unresponsive after activate | Medium | 10s timeout. Board may need JTAG recovery if boot manager itself is faulty. |
| Bank 3 metadata corrupt | Low | Boot manager checks magic + CRC before any action |

---

## 11. What Has Been Built So Far

| Function | File | Status |
|---|---|---|
| `FW_eraseFlashRange(bankBaseAddress, sectorCount)` | `fw_update_flash.c` | Done |
| `FW_writeTestPattern(bankBaseAddress, sectorCount)` | `fw_update_flash.c` | Done (test only) |
| `FW_testBank2()` | `fw_update_flash.c` | Stub — ready to fill |

---

## 12. Immediate Next Steps

1. **Update linker `.cmd` file** — reserve sectors 0–3 for boot manager, move app entry to `0x081000`
2. **Build Project 0** — boot manager (separate CCS project, ~50 lines)
3. **Complete Project 1** — `FW_programBlock`, `FW_calculateCRC`, `FW_verifyCRC`, `FW_triggerUpdate`, `FW_bankSwap`
4. **Test Project 1** — erase Bank 2, write test data, verify CRC, trigger boot manager copy via CCS
5. **Build Project 2** — BU-Board firmware update receiver state machine
6. **Build Projects 3–5** — S-Board sender, image receiver, self-update
7. **Integration and full system test**   