# Plan B — Application-Level Firmware Update

## How It Works

The running application receives new firmware over CAN, writes it to a **staging flash bank**, verifies CRC, then a tiny RAM-resident function copies staging → active bank and resets. No ROM bootloader or flash kernel involved.

```
Running App receives data over CAN → Writes to Bank 2 (staging) → Verifies CRC
→ Copies bankSwap() to RAM → Jumps to RAM → Erases Bank 0 → Copies Bank 2 → Bank 0 → Reset
→ Boots new firmware from Bank 0
```

---

## Protocol (Custom, ACK-Based)

### Frame Format (CAN-FD, 64 bytes)

```
Byte 0:      Command code
Byte 1:      Target board ID (0xFF = broadcast)
Bytes 2-3:   Sequence number (for data frames)
Bytes 4-63:  Payload (60 bytes of firmware data)
```

### Commands

```c
// M-Board → S-Board (over MCANB, CAN ID 3)
CMD_FW_TRANSFER_START   = 0x30   // Begin image transfer session
CMD_FW_IMAGE_HEADER     = 0x31   // Image metadata (type, size, CRC, version)
CMD_FW_IMAGE_DATA       = 0x32   // Firmware data (60 bytes payload per frame)
CMD_FW_IMAGE_COMPLETE   = 0x33   // End of one image
CMD_FW_TRANSFER_DONE    = 0x34   // All images sent
CMD_FW_START_BU_UPDATE  = 0x35   // Trigger BU flashing sequence
CMD_FW_START_SELF_UPD   = 0x36   // Trigger S-Board self-update
CMD_FW_STATUS_REQUEST   = 0x37   // Poll update progress

// S-Board → BU-Board (over MCANA, CAN ID 4 broadcast)
CMD_FW_UPDATE_PREPARE   = 0x20   // Prepare target BU for update
CMD_FW_DATA             = 0x21   // Firmware data (60 bytes payload per frame)
CMD_FW_VERIFY           = 0x22   // Request CRC verification of staging bank
CMD_FW_ACTIVATE         = 0x23   // Trigger bank swap
CMD_FW_ABORT            = 0x24   // Abort update, resume normal operation

// BU-Board → S-Board (over MCANA, BU's own CAN ID 11-28)
CMD_FW_ACK              = 0x25   // Acknowledge (includes last seq# received)
CMD_FW_NAK              = 0x26   // Error (includes error code)
CMD_FW_VERIFY_PASS      = 0x27   // CRC match confirmed
CMD_FW_VERIFY_FAIL      = 0x28   // CRC mismatch
CMD_FW_BOOT_OK          = 0x29   // New firmware booted successfully (includes version)
```

### Flow Control

- Sender transmits burst of **16 frames** (16 x 60 = 960 bytes of payload)
- Receiver ACKs with sequence number of last frame received
- Sender waits up to **500ms** for ACK
- If no ACK → retransmit entire burst (up to **3 retries**)
- After 3 failures → declare board update FAILED, move to next board
- **Failed board is NOT bricked** — it's still running old firmware, staging bank just has partial data

### Why 16-Frame Bursts?

- BU-Board programs flash 512-bit (32 words = 64 bytes) at a time
- Each program operation takes ~100us
- 16 frames x 60 bytes = 960 bytes = 15 flash writes
- 15 x 100us = 1.5ms — easily fits in the time between bursts
- ACK round-trip adds ~1ms on CAN-FD
- Effective throughput: ~960 bytes per ~3ms = **~320 KB/s**

---

## S-Board Flash Memory Layout

```
Bank 0: 0x080000 (128KB) — S-Board running application (~82KB used)
Bank 1: 0x0A0000 (128KB) — BU firmware image, raw binary (~38KB stored)
Bank 2: 0x0C0000 (128KB) — New S-Board firmware image, raw binary (~82KB stored)
Bank 3: 0x0E0000 (128KB) — Image metadata + spare
Bank 4: 0x100000  (32KB) — Calibration data (unchanged)
```

**Raw binary format** — no SCI8 hex overhead. Stored as-is from the .out file (converted by M-Board).

### Image Metadata (Bank 3, First Sector)

```c
typedef struct {
    uint16_t magic;           // 0xFW01 — validates structure
    uint16_t imageType;       // 0 = BU firmware, 1 = S-Board firmware
    uint32_t imageSize;       // Size in 16-bit words
    uint32_t imageCRC;        // CRC32 of image data
    uint32_t version;         // Firmware version number
    uint32_t destBank;        // Target bank start address
    uint32_t entryPoint;      // Application entry address (0x080000)
    uint16_t valid;           // 0xA5A5 = image received and CRC verified
    uint16_t activated;       // 0x5A5A = image has been flashed to Bank 0
} FW_ImageHeader;
```

Two headers stored: one for BU image, one for S-Board image.

### Storage Budget

```
BU firmware:      ~38KB raw → Bank 1 (128KB) → 70% spare room
S-Board firmware: ~82KB raw → Bank 2 (128KB) → 36% spare room
Metadata:         ~32 words → Bank 3 (128KB) → 99.9% spare room
Total:            ~120KB used out of 384KB available (Banks 1-3)
```

Even if firmware triples in size, still fits comfortably.

---

## BU-Board Flash Memory Layout

```
Bank 0: 0x080000 (128KB) — Running application (~38KB used)
Bank 1: 0x0A0000 (128KB) — Reserved for future code growth
Bank 2: 0x0C0000 (128KB) — STAGING: new firmware received from S-Board
Bank 3: 0x0E0000 (128KB) — Spare
Bank 4: 0x100000  (32KB) — Calibration data (unchanged)
```

---

## Complete Update Sequence

### Phase 0: M-Board → S-Board (Image Transfer, ~5 seconds)

```
M-Board                                    S-Board
   |                                           |
   |-- CMD_FW_TRANSFER_START ----------------->|
   |                                           | Erase Banks 1, 2, 3
   |<-------------- ACK (ready) --------------|
   |                                           |
   |-- CMD_FW_IMAGE_HEADER (BU, 38KB, CRC) -->|
   |<-------------- ACK ----------------------|
   |                                           | Store metadata in Bank 3
   |                                           |
   |-- CMD_FW_IMAGE_DATA (seq 0-15) --------->| Write to Bank 1
   |<-------------- ACK (seq 15) -------------|
   |-- CMD_FW_IMAGE_DATA (seq 16-31) -------->|
   |<-------------- ACK (seq 31) -------------|
   |-- ... (~634 frames total for 38KB) ------>|
   |-- CMD_FW_IMAGE_COMPLETE ----------------->|
   |                                           | Verify CRC of Bank 1
   |<-------------- VERIFY_PASS --------------|
   |                                           |
   |-- CMD_FW_IMAGE_HEADER (S-Board, 82KB) -->|
   |<-------------- ACK ----------------------|
   |                                           | Store metadata in Bank 3
   |                                           |
   |-- CMD_FW_IMAGE_DATA (seq 0-15) --------->| Write to Bank 2
   |<-------------- ACK (seq 15) -------------|
   |-- ... (~1367 frames total for 82KB) ----->|
   |-- CMD_FW_IMAGE_COMPLETE ----------------->|
   |                                           | Verify CRC of Bank 2
   |<-------------- VERIFY_PASS --------------|
   |                                           |
   |-- CMD_FW_TRANSFER_DONE ----------------->|
   |<-------------- ACK (both images OK) -----|
```

### Phase 1: S-Board → 18 BU-Boards (~54 seconds)

Sequential, one board at a time. Non-target boards continue normal operation.

```
For each BU board (board_id = 11 to 28):

S-Board                                    BU-Board #N
   |                                           |
   |-- CMD_FW_UPDATE_PREPARE (board_id) ------>|
   |                                           | Stop normal operation
   |                                           | Erase Bank 2 (staging)
   |<-------------- ACK (ready, bank size) ----|
   |                                           |
   |-- CMD_FW_DATA (seq 0, 60 bytes) -------->| Write to Bank 2
   |-- CMD_FW_DATA (seq 1, 60 bytes) -------->|
   |-- ... (burst of 16 frames)                |
   |-- CMD_FW_DATA (seq 15, 60 bytes) ------->|
   |<-------------- ACK (seq 15) -------------|
   |                                           |
   |-- CMD_FW_DATA (seq 16-31) --------------->|
   |<-------------- ACK (seq 31) -------------|
   |                                           |
   |-- ... (~634 frames for 38KB) ------------>|
   |                                           |
   |-- CMD_FW_VERIFY (CRC32, size) ----------->|
   |                                           | CRC32 over Bank 2 content
   |<-------------- VERIFY_PASS --------------|
   |                                           |
   |-- CMD_FW_ACTIVATE ----------------------->|
   |                                           |
   |                                           | [In BU-Board:]
   |                                           | 1. Copy bankSwap() to RAM
   |                                           | 2. DINT (disable interrupts)
   |                                           | 3. Jump to RAM
   |                                           | 4. Erase Bank 0
   |                                           | 5. Copy Bank 2 → Bank 0
   |                                           |    (512-bit writes, ~600 iterations)
   |                                           | 6. SysCtl_resetDevice()
   |                                           |
   |         ~2 second wait                    |
   |                                           | New firmware boots
   |                                           | Sends heartbeat on CAN ID (11-28)
   |<-------------- CMD_FW_BOOT_OK (ver) -----|
   |                                           |
   |   Log: "BU #N: OK, version X"            |
   |-- Report status to M-Board                |
```

**Time per BU board**:
- Prepare + erase staging: ~0.5s
- Data transfer: 38KB / 320 KB/s = ~0.12s + ACK overhead = ~0.5s
- CRC verify: ~0.1s
- Bank swap (erase + copy): ~1.5s
- Boot + confirm: ~0.5s
- **Total: ~3 seconds per board**

**Total for 18 BU boards: ~54 seconds**

### Phase 3: S-Board Self-Update (~3 seconds)

After all 18 BU boards confirmed:

```
S-Board                                    M-Board
   |                                           |
   |-- "All 18 BU OK, self-updating" -------->|
   |                                           |
   | [S-Board executes:]
   | 1. Verify Bank 2 metadata (magic, CRC, valid flag)
   | 2. Copy bankSwap() function to RAM
   | 3. Send final "SELF_UPDATE_STARTING" to M-Board
   | 4. DINT (disable all interrupts)
   | 5. Jump to RAM-resident bankSwap()
   |    a. Erase Bank 0 (~0.5s)
   |    b. Copy Bank 2 → Bank 0, 512-bit writes (~1s for 82KB)
   |    c. SysCtl_resetDevice()
   |
   |         ~3 second silence                 |
   |                                           |
   | New S-Board firmware boots                |
   | Re-discovers BU boards                    |
   |                                           |
   |-- "Update complete, version X" --------->|
```

### Total Update Time

```
Phase 0: Image transfer M-Board → S-Board:     ~5 seconds
Phase 1: Flash 18 BU boards (sequential):      ~54 seconds
Phase 2: S-Board self-update:                   ~3 seconds
                                          ─────────────────
TOTAL:                                         ~62 seconds
```

---

## What Needs to Be Built (6 Projects)

### Project 1: Flash Writer Library (Common, used by both boards)

**Purpose**: Reusable flash erase/program/verify for any bank + RAM-resident bank swap

**Files**: `fw_update_flash.c`, `fw_update_flash.h`

**Functions**:
```c
// Initialize Flash API for firmware update operations
void FW_Flash_init(void);

// Erase an entire flash bank
// bankNum: 0-4, returns: 0=success, nonzero=error
uint16_t FW_eraseBank(uint16_t bankNum);

// Program a 512-bit (32-word) aligned block
// destAddr: flash destination (must be 32-word aligned)
// srcBuf: pointer to 32 words of data
// returns: 0=success, nonzero=error
uint16_t FW_programBlock(uint32_t destAddr, uint16_t* srcBuf);

// Verify CRC32 over a flash region
// startAddr: beginning of region
// numWords: number of 16-bit words
// expectedCRC: what it should be
// returns: 0=match, 1=mismatch
uint16_t FW_verifyCRC(uint32_t startAddr, uint32_t numWords, uint32_t expectedCRC);

// Calculate CRC32 over a flash region
uint32_t FW_calculateCRC(uint32_t startAddr, uint32_t numWords);

// RAM-RESIDENT bank swap — NEVER RETURNS
// #pragma CODE_SECTION(FW_bankSwap, ".TI.ramfunc")
// Erases dstBank, copies srcBank → dstBank, resets device
void FW_bankSwap(uint32_t srcBankAddr, uint32_t dstBankAddr, uint32_t numWords);
```

**Dependencies**: `FAPI_F28P55x_EABI_v4.00.00.lib`

**Critical detail**: `FW_bankSwap()` MUST be in `.TI.ramfunc` section (copied to RAM at startup). It disables interrupts, erases the destination bank (which may be the running code bank), copies data, and resets. It never returns.

**Test plan**: 
- Load on S-Board via JTAG
- From CCS: call `FW_eraseBank(2)` → verify Bank 2 is blank
- Write test pattern to Bank 2 → verify with `FW_verifyCRC()`
- Call `FW_bankSwap(Bank2, Bank0, testSize)` → verify device resets and Bank 0 has test pattern

### Project 2: BU-Board Firmware Update Receiver

**Purpose**: BU-Board CAN command handler that receives firmware, writes to staging bank, activates

**Files**: `fw_update_bu.c`, `fw_update_bu.h`

**Functions**:
```c
// Initialize firmware update module
void FWUpdate_init(void);

// Process incoming firmware update command (called from main loop when update cmd received)
void FWUpdate_processCommand(uint8_t cmd, uint8_t* data, uint16_t len);

// State machine states
typedef enum {
    FW_STATE_IDLE,           // Normal operation, waiting for PREPARE
    FW_STATE_PREPARING,      // Erasing staging bank
    FW_STATE_RECEIVING,      // Receiving data frames, writing to flash
    FW_STATE_VERIFYING,      // CRC check on staging bank
    FW_STATE_ACTIVATING      // Bank swap in progress (point of no return)
} FWUpdate_State;

// Get current state (for main loop to check)
FWUpdate_State FWUpdate_getState(void);
```

**Internal flow**:
1. `CMD_FW_UPDATE_PREPARE` → set state to PREPARING, erase Bank 2, ACK when done
2. `CMD_FW_DATA` → write 60-byte payload to Bank 2 at correct offset, ACK every 16th frame
3. `CMD_FW_VERIFY` → calculate CRC32 over written region, compare, send PASS/FAIL
4. `CMD_FW_ACTIVATE` → call `FW_bankSwap(Bank2Addr, Bank0Addr, imageSize)`
5. `CMD_FW_ABORT` → erase Bank 2, return to IDLE state (resume normal operation)

**Dependencies**: Project 1, Flash API lib added to BU build

**Test plan**:
- Load on BU-Board via JTAG
- From CCS scripting or S-Board test: send PREPARE → verify Bank 2 erased
- Send a small test pattern (e.g., 1KB) as FW_DATA frames → verify written to Bank 2
- Send VERIFY with correct CRC → expect PASS
- Send ACTIVATE → verify BU resets and Bank 0 has the test pattern

### Project 3: S-Board CAN Flash Master (Sender to BU)

**Purpose**: S-Board reads BU firmware from Bank 1 and streams to target BU-Board

**Files**: `fw_master.c`, `fw_master.h`

**Functions**:
```c
// Flash one BU board. Returns 0=success, error code on failure.
uint16_t FWMaster_flashOneBU(uint16_t boardID);

// Flash all active BU boards sequentially. Returns bitmask of failed boards.
uint32_t FWMaster_flashAllBU(void);

// Get progress info
typedef struct {
    uint16_t currentBoard;    // Board ID being flashed (0 if idle)
    uint16_t totalBoards;     // Total boards to flash
    uint16_t boardsDone;      // Boards successfully completed
    uint16_t boardsFailed;    // Boards that failed
    uint16_t framesSent;      // Frames sent for current board
    uint16_t framesTotal;     // Total frames for current board
} FWMaster_Progress;

void FWMaster_getProgress(FWMaster_Progress* progress);
```

**Internal flow for `FWMaster_flashOneBU(boardID)`**:
1. Send CMD_FW_UPDATE_PREPARE to boardID → wait for ACK (timeout 5s)
2. Read image metadata from Bank 3 (size, CRC)
3. Loop: read 60 bytes from Bank 1, pack into CMD_FW_DATA frame, send
   - Every 16 frames: wait for ACK (timeout 500ms, retry 3x)
4. Send CMD_FW_VERIFY with CRC and size → wait for VERIFY_PASS (timeout 5s)
5. Send CMD_FW_ACTIVATE → wait for CMD_FW_BOOT_OK (timeout 10s)
6. Return success/failure

**Dependencies**: Project 1, existing S-Board CAN driver, Bank 1 must have valid BU image

**Test plan**:
- Pre-load a small test binary into S-Board Bank 1 via CCS
- Connect one BU-Board running Project 2
- Call `FWMaster_flashOneBU(11)` from CCS
- Verify BU-Board gets new firmware

### Project 4: S-Board Image Receiver (from M-Board)

**Purpose**: S-Board receives firmware images from M-Board over MCANB, stores in Banks 1+2

**Files**: `fw_image_rx.c`, `fw_image_rx.h`

**Functions**:
```c
// Start a new transfer session (erases staging banks)
uint16_t ImageRx_startTransfer(void);

// Process a received M-Board firmware command
void ImageRx_processCommand(uint8_t cmd, uint8_t* data, uint16_t len);

// Check if both images are received and verified
bool ImageRx_isReady(void);

// Get image info
typedef struct {
    uint32_t size;
    uint32_t crc;
    uint32_t version;
    bool     valid;
} ImageRx_Info;

void ImageRx_getBUImageInfo(ImageRx_Info* info);
void ImageRx_getSBoardImageInfo(ImageRx_Info* info);
```

**Dependencies**: Project 1, existing MCANB driver

**Test plan**:
- Simulate M-Board by sending CAN frames from CCS or a Python script over a PCAN adapter
- Send CMD_FW_TRANSFER_START → verify Banks 1,2,3 erased
- Send BU image header + data → verify stored in Bank 1
- Send S-Board image header + data → verify stored in Bank 2
- Call `ImageRx_isReady()` → expect true

### Project 5: S-Board Self-Update

**Purpose**: S-Board updates its own Bank 0 from Bank 2

**Files**: `fw_self_update.c`, `fw_self_update.h`

**Functions**:
```c
// Execute self-update. DOES NOT RETURN on success.
// Returns error code only on pre-check failure (bad image, CRC mismatch)
uint16_t SelfUpdate_execute(void);
```

**Internal flow**:
1. Read S-Board image metadata from Bank 3
2. Verify magic, valid flag, CRC of Bank 2 content
3. If any check fails → return error (do NOT proceed)
4. Send "SELF_UPDATE_STARTING" to M-Board
5. 10ms delay (let CAN frame transmit)
6. Call `FW_bankSwap(Bank2Addr, Bank0Addr, imageSize)` → never returns

**Dependencies**: Project 1

**Test plan**:
- Build a slightly modified S-Board firmware (different version string)
- Load original firmware via JTAG
- Pre-load modified firmware into Bank 2 + correct metadata in Bank 3 via CCS
- Call `SelfUpdate_execute()`
- Verify S-Board reboots with new version string

### Project 6: Integration

**Purpose**: Wire Projects 1-5 into main S-Board and BU-Board firmware

**S-Board additions**:
- New M-Board command handlers in main loop (CMD_FW_TRANSFER_START, etc.)
- State machine: `FW_IDLE → FW_RECEIVING → FW_BU_UPDATING → FW_SELF_UPDATING → FW_DONE`
- Progress reporting to M-Board (responds to CMD_FW_STATUS_REQUEST)

**BU-Board additions**:
- New command handler in main loop CAN ISR/processing
- When in FW_STATE_RECEIVING, stop normal ADC/power operations to free CPU for flash writes
- After successful boot, send CMD_FW_BOOT_OK with new version

---

## Risks and Mitigations

| Risk | Severity | Mitigation |
|------|----------|------------|
| **Power loss during bank swap** | High — ~1.5s window, board bricked | UPS/capacitor hold-up recommended. Factory OTP BOOTDEF with MCAN recovery as last resort |
| **CAN frame loss** | Low — ACK protocol detects and retransmits | 3 retries per burst. If all fail, board stays on old firmware (not bricked) |
| **Flash API adds ~40KB to BU code** | Low — BU Bank 0 has 90KB spare | Confirmed it fits |
| **Staging bank CRC mismatch** | Low — detected before activation | Retry transfer or abort. Board stays on old firmware |
| **BU board unresponsive after activate** | Medium — bank swap might have failed | Wait 10s timeout, report failure to M-Board. Board may need JTAG recovery |
| **18 boards take too long** | Low — ~54s total | Acceptable for firmware update. Could parallelize 2-3 boards if needed (different CAN IDs) |

---

## Pros and Cons Summary

### Pros
- **ACK-based reliable protocol** — no silent corruption, retransmit on failure
- **CRC verification before activation** — never flash a bad image
- **Failed transfer = board keeps old firmware** — not bricked
- **Progress reporting** — M-Board knows exactly which board, which frame
- **Raw binary format** — no 2x SCI8 overhead, everything fits in one bank each
- **Single protocol** for both S-Board and BU-Board updates
- **Fast**: ~62 seconds total for all 19 boards
- **Abort capability** — can cancel mid-transfer, board resumes normal operation
- **No M-Board flash protocol** — M-Board just sends raw data, S-Board handles everything

### Cons
- **Flash API library (~40KB) needed in BU-Board** — added to build
- **More code to write** — ~6 files, ~1500-2000 lines total across all projects
- **RAM-resident bankSwap()** — must be carefully tested, ~1.5s vulnerability window
- **BU-Board stops normal operation during update** — 18 boards updated one at a time, so 17 keep running
