# S-Board Calibration System - Complete Reference

## Table of Contents

1. [Overview](#1-overview)
2. [Data Structures](#2-data-structures)
3. [Flash Storage](#3-flash-storage)
4. [CAN Bus Configuration](#4-can-bus-configuration)
5. [Command Protocol](#5-command-protocol)
6. [Discovery Flow](#6-discovery-flow)
7. [Calibration Data Flow](#7-calibration-data-flow)
8. [M-Board Communication](#8-m-board-communication)
9. [BU Board Calibration Send](#9-bu-board-calibration-send)
10. [Flash Operations](#10-flash-operations)
11. [CRC Validation](#11-crc-validation)
12. [Timer System](#12-timer-system)
13. [ISR Message Routing](#13-isr-message-routing)
14. [Main Loop State Machine](#14-main-loop-state-machine)
15. [Acknowledgment Frames](#15-acknowledgment-frames)
16. [Default Values](#16-default-values)
17. [State Variables Reference](#17-state-variables-reference)
18. [File Reference](#18-file-reference)

---

## 1. Overview

The calibration system manages CT current gains and kW power gains for up to 12 BU boards
(18 CTs each = 216 total channels). Calibration data is persisted in flash with wear-leveling
and exchanged between M-Board, S-Board, and BU boards over CAN-FD.

```
M-Board (CAN ID 3)                    S-Board (CAN ID 5)
    |                                      |
    |--- CMD_START_DISCOVERY (0x05) ------>|
    |<-- Discovery Start ACK (0x05,0x01) --|
    |                                      |--- DISCOVERY_START (0x0B) ---> BU Boards
    |                                      |<-- BU responses (ID 11-22) ---|
    |                                      |--- DISCOVERY_STOP (0x0C) ---> BU Boards
    |                                      |
    |                                      | (load flash data, prepare frames)
    |                                      |
    |<-- 15 calibration frames (ID 0x2) ---|
    |--- retransmit request (0x03) ------->| (if frames missing)
    |<-- missing frames (ID 0x2) ----------|
```

---

## 2. Data Structures

**File: `pdu_manager.h`**

### CalibrationGains
```c
typedef struct {
    uint16_t ctGain[216];   // CT current calibration gains (numberOfCTs = 216)
    uint16_t kwGain[216];   // Power calibration gains
} CalibrationGains;         // 864 bytes
```

### ThreePhaseValues
```c
typedef struct {
    uint16_t R;             // R phase
    uint16_t Y;             // Y phase
    uint16_t B;             // B phase
} ThreePhaseValues;         // 6 bytes
```

### PrimaryMeasurements
```c
typedef struct {
    ThreePhaseValues voltage;   // 6 bytes
    ThreePhaseValues current;   // 6 bytes
    uint16_t neutralCurrent;    // 2 bytes
    uint16_t totalKW;           // 2 bytes
} PrimaryMeasurements;          // 16 bytes
```

### SecondaryMeasurements
```c
typedef struct {
    ThreePhaseValues voltage;   // 6 bytes
    ThreePhaseValues current;   // 6 bytes
    uint16_t neutralCurrent;    // 2 bytes
    ThreePhaseValues kw;        // 6 bytes
} SecondaryMeasurements;        // 20 bytes
```

### PDUData
```c
struct PDUData {
    CalibrationGains gains;          // 864 bytes
    PrimaryMeasurements primary;     // 16 bytes
    SecondaryMeasurements secondary; // 20 bytes
    uint16_t groundCurrent;          // 2 bytes
};                                   // 902 bytes total
```

### CalibrationData (what gets written to flash)
```c
struct CalibrationData {
    uint16_t sectorIndex;   // Wear-leveling sequence number (2 bytes)
    PDUData pduData;        // 902 bytes
    uint16_t crc;           // CRC-16 checksum (2 bytes)
};                          // 906 bytes total (~453 uint16_t words)
```

### PDUDataManager (runtime manager)
```c
struct PDUDataManager {
    CalibrationData readWriteData;  // For flash read/write operations
    CalibrationData workingData;    // For active PDU operations
    bool dataSync;                  // True when both structures match
};
```

### CANFrame
```c
struct CANFrame {
    uint8_t data[64];       // 64-byte CAN-FD frame payload
    uint16_t order;         // Frame sequence number (1-15)
};
```

### BUBoardData (per BU board)
```c
typedef struct {
    uint16_t currentGains[18];      // 18 CT current gains
    uint16_t kwGains[18];           // 18 kW gains
    bool currentFrameReceived;
    bool kwFrameReceived;
    bool isActive;
} BUBoardData;
```

### BoardAckStatus (per BU board)
```c
typedef struct {
    bool currentGainAck;
    bool kwGainAck;
    uint32_t retryCount;
    bool transmissionComplete;
} BoardAckStatus;
```

---

## 3. Flash Storage

**File: `flash_driver.c/h`**

### Memory Map
| Parameter         | Value        | Notes                              |
|-------------------|--------------|------------------------------------|
| Bank 4 Start      | `0x100000`   | First sector address               |
| Bank 4 End        | `0x180000`   | Last sector address                |
| Sector Size       | `0x400`      | 1024 bytes (1 KB)                  |
| Total Sectors     | ~512         | (0x80000 / 0x400)                  |
| Wrap Limit        | `0xFFFE`     | Max sector index before bank erase |
| Words per Buffer  | `0x400`      | 1024 uint16_t words                |

### Wear-Leveling Algorithm
Each sector's first uint16_t word is a **sequence number** (`sectorIndex`):
- `0xFFFF` = empty sector (erased)
- `1` to `0xFFFE` = valid data, higher = newer
- `0x0000` = invalid/corrupt

**`findReadAndWriteAddress()`** scans all sectors:
- `readAddress` = sector with highest `sectorIndex` (newest data)
- `writeAddress` = sector with lowest `sectorIndex` (oldest, next to erase)
- `newestData` = highest sequence number found
- `oldestData` = lowest sequence number found
- `emptySectorFound` = true if any 0xFFFF sector exists

### Write Strategy
1. If empty sector exists: use it (no erase needed)
2. If no empty sector: erase oldest sector and reuse
3. If sequence reaches `0xFFFE`: erase entire Bank 4, reset to 0

---

## 4. CAN Bus Configuration

**File: `can_driver.c`, `common.h`**

### Two CAN Modules
| Module | Connection | Purpose                    |
|--------|------------|----------------------------|
| MCANA  | BU boards  | Discovery, calibration TX/RX to BU boards |
| MCANB  | M-Board    | Commands RX, calibration TX to M-Board    |

### CAN-FD Frame Parameters
| Parameter | Value  | Meaning                 |
|-----------|--------|-------------------------|
| FDF       | 1      | CAN-FD format           |
| BRS       | 1      | Bit rate switching on   |
| DLC       | 15     | 64-byte payload         |
| EFC       | 1      | Event FIFO enabled      |
| MM        | 0xAA   | Message marker          |

### GPIO Pin Assignment
| Pin    | Function    |
|--------|-------------|
| GPIO 0 | MCANA_RX    |
| GPIO 1 | MCANA_TX    |
| GPIO 2 | MCANB_TX    |
| GPIO 3 | MCANB_RX    |

### CAN ID Assignments
| CAN ID | Decimal | Direction | Purpose                          |
|--------|---------|-----------|----------------------------------|
| 0x02   | 2       | S -> M    | Calibration data frames to M-Board |
| 0x03   | 3       | M -> S    | M-Board commands to S-Board      |
| 0x04   | 4       | S -> BU   | Broadcasts (discovery, ACKs, calibration to BU) |
| 0x05   | 5       | S -> BU   | Voltage data to BU boards        |
| 0x06   | 6       | S -> BU   | Y-phase voltage                  |
| 0x07   | 7       | S -> BU   | B-phase voltage                  |
| 0x0B   | 11      | BU -> S   | BU Board 1 (first BU ID)        |
| ...    | 12-21   | BU -> S   | BU Boards 2-11                   |
| 0x16   | 22      | BU -> S   | BU Board 12 (last BU ID)        |

### Message RAM Layout
| Resource             | Count | Element Size |
|----------------------|-------|--------------|
| Standard ID Filters  | 35    | -            |
| RX FIFO 0            | 15    | 64 bytes     |
| RX Buffers           | 19    | 64 bytes     |
| TX Buffers           | 32    | 64 bytes     |
| TX Event FIFO        | 32    | -            |

---

## 5. Command Protocol

**File: `common.h`**

### M-Board to S-Board Commands (received on MCANB, CAN ID 3)

| Cmd  | Hex  | Constant             | Description                          |
|------|------|----------------------|--------------------------------------|
| 0x01 | 0x01 | `CMD_VERSION_CHECK`  | Request calibration format version   |
| 0x02 | 0x02 | `CMD_FLASH_ENABLE`   | Enable flash write (password required) |
| 0x03 | 0x03 | `CMD_READ`           | Read calibration / retransmission request |
| 0x05 | 0x05 | `CMD_START_DISCOVERY`| Start BU board discovery + calibration |
| 0x09 | 0x09 | `CMD_ERASE_BANK4`   | Erase entire flash Bank 4            |
| 0x0A | 0x0A | `CMD_WRITE_DEFAULT`  | Write default calibration to flash   |
| 0xFD | 0xFD | _(runtime)_          | Runtime data request                 |

### S-Board to BU Board Broadcasts (sent on MCANA, CAN ID 4)

| Cmd  | Hex  | Constant                | Description                    |
|------|------|-------------------------|--------------------------------|
| 0x0B | 0x0B | `DISCOVERY_START_CMD`   | Start discovery phase          |
| 0x0C | 0x0C | `DISCOVERY_STOP_CMD`    | End discovery phase            |
| 0x0D | 0x0D | `DISCOVERY_RESPONSE_ACK`| Acknowledge BU discovery response |

### Status Codes
| Code | Constant      | Meaning |
|------|---------------|---------|
| 0x00 | `STATUS_FAIL` | Failure |
| 0x01 | `STATUS_OK`   | Success |

---

## 6. Discovery Flow

**Files: `bu_board.c/h`, `adc_ex2_soc_epwm.c`**

### Trigger
M-Board sends `CMD_START_DISCOVERY (0x05)` on MCANB (CAN ID 3).

### Step-by-Step

```
Step 1: M-Board sends CMD_START_DISCOVERY (0x05)
        ↓
Step 2: S-Board receives on MCANB RX Buffer (boardId == 3)
        → ISR sets CommandReceived = true
        ↓
Step 3: Main loop handles command:
        → sendDiscoveryStartAck()    // ACK to M-Board on MCANB ID 0x4
        → startNewDiscovery()        // Reset + start
        ↓
Step 4: startNewDiscovery() executes:
        → stop10SecTimer()
        → Reset: activeBUMask=0, discoveryMask=0, numActiveBUBoards=0
        → Reset: buDataCollectionComplete=false, buDataProcessed=false
        → Reset: receivedCurrentFrames=0, receivedKWFrames=0
        → memset(buBoards, 0, ...)
        → startBUDiscovery()
        ↓
Step 5: startBUDiscovery() executes:
        → discoveryInProgress = true
        → initDiscoveryTracking()    // Clear discovered boards info
        → sendDiscoveryStartMessage()  // Broadcast 0x0B on MCANA ID 0x4
        → restart10SecTimer()        // Start 10-second timer
        ↓
Step 6: BU boards respond (10-second window)
        → Each BU board sends response on its CAN ID (11-22)
        → ISR stores in rxMsg[], sets buMessagePending[i]=true, buMessageReceived=true
        → Main loop: processBUMessages()
        → Since discoveryInProgress==true → processBUDiscoveryResponse(boardId)
        → Updates discoveryMask, sends DISCOVERY_RESPONSE_ACK (0x0D) per board
        ↓
Step 7: Timer expires (timer10SecExpired = true)
        ↓
Step 8: Main loop detects: discoveryInProgress && timer10SecExpired
        → finalizeBUDiscovery()
           → discoveryInProgress = false
           → activeBUMask |= discoveryMask
           → Count numActiveBUBoards
           → sendDiscoveryStopMessage()  // Broadcast 0x0C on MCANA ID 0x4
           → stop10SecTimer()
        → copyFlashToBUBoards()  (if activeBUMask != 0)
        → discoveryComplete = true
        → buDataCollectionComplete = true
        → buDataProcessed = true
        → sendToMBoard = true
        ↓
Step 9: Send calibration to M-Board (see Section 8)
```

### Discovery Response ACK Frame Format
Sent per BU board on MCANA, CAN ID 0x4:
```
Byte 0: 0x0D (DISCOVERY_RESPONSE_ACK)
Byte 1: 0x64 + boardId - 0x5 (board indicator)
Byte 2: 0x01 (STATUS_OK)
Bytes 3-63: 0x00
```

### Duplicate ID Detection
If a board that was already acknowledged responds again:
- `duplicateIDFound = true`
- `sendDuplicateIDError(boardID)` sends error frame on MCANB (CAN ID 0x4):
  ```
  Byte 0: 0x0F (error command)
  Byte 1: boardID
  ```

---

## 7. Calibration Data Flow

### After Discovery Completes (no BU boards or with BU boards)

```
Discovery Complete
    |
    ├── activeBUMask != 0 (BU boards found)
    │   → copyFlashToBUBoards()
    │     → Load gains from pduManager.workingData into buBoards[]
    │     → Set receivedCurrentFrames = activeBUMask
    │     → Set receivedKWFrames = activeBUMask
    │
    ├── activeBUMask == 0 (no BU boards)
    │   → Skip copyFlashToBUBoards() (returns early)
    │
    ├── Set discoveryComplete = true
    ├── Set buDataCollectionComplete = true
    ├── Set buDataProcessed = true
    ├── Set sendToMBoard = true
    │
    └── Next iteration: sendCalibrationDataToMBoard()
        → prepareFramesForTransmission(pduManager.readWriteData)
        → Send 15 frames on MCANB, CAN ID 0x2
```

**Key point:** Even with zero BU boards, the latest flash data
(`pduManager.readWriteData` loaded at startup) is sent to M-Board.
The S-Board never gets stuck.

### Data Source at Startup
```c
// In startup():
if (newestData == 0) {
    // No flash data → write defaults → program flash
    writeDefaultCalibrationValues();
    Example_ProgramUsingAutoECC();
    syncReadWriteToWorking();
} else {
    // Load existing flash data
    memcpy(&pduManager.readWriteData, (void*)readAddress, sizeof(CalibrationData));
    syncReadWriteToWorking();
}
```

---

## 8. M-Board Communication

**File: `m_board.c`**

### sendCalibrationDataToMBoard()

**Pre-conditions (all must be true):**
- `discoveryComplete == true`
- `buDataCollectionComplete == true`
- `buDataProcessed == true`
- `sendToMBoard == true`

**Process:**
1. Verify `buDataCollectionComplete && buDataProcessed`
2. If `!pduManager.dataSync` → `syncReadWriteToWorking()`
3. `prepareFramesForTransmission()` chunks `pduManager.readWriteData` into 15 frames
4. Send 15 frames sequentially on MCANB, CAN ID 0x2
5. 500us delay between each frame
6. Reset `sendToMBoard = false`

### Frame Format (S-Board to M-Board)
Each of the 15 frames is a 64-byte CAN-FD message:
```
Byte 0:    Frame order high byte (0x00 for frames 1-15)
Byte 1:    Frame order low byte (0x01 to 0x0F)
Bytes 2-63: Up to 31 x uint16_t values in big-endian (high byte first)
```

**Data distribution across 15 frames (451 uint16_t words + sectorIndex + CRC):**

| Frame | Words | Content                                    |
|-------|-------|--------------------------------------------|
| 1     | 31    | sectorIndex + ctGain[0..29]                |
| 2     | 31    | ctGain[30..60]                             |
| 3     | 31    | ctGain[61..91]                             |
| ...   | ...   | ctGain continued, then kwGain              |
| 8     | 31    | kwGain values                              |
| ...   | ...   | kwGain continued                           |
| 14    | 31    | kwGain end + measurements                  |
| 15    | ~18   | Remaining measurements + groundCurrent + CRC |

### Retransmission Protocol
If M-Board didn't receive all 15 frames, it sends CMD_READ (0x03):
```
Byte 0:    0x03 (retransmit command)
Bytes 1-15: Bitmask per frame (0x00 = missing, 0x01 = received)
```
S-Board retransmits only the missing frames via `sendMissingFrames()`.

### Version Check Response
Sent on MCANB, CAN ID 0x4:
```
Byte 0: 0x01 (CMD_VERSION_CHECK echo)
Byte 1: 1    (majorVersionNumber)
Byte 2: 0    (minorVersionNumber)
```

---

## 9. BU Board Calibration Send

**File: `bu_board.c`**

### sendBoardCalibrationData(boardIndex)

Sends 2 CAN-FD frames per BU board on MCANA:

**Frame 1 - Current Gains:**
```
CAN ID: boardIndex + 11 (FIRST_BU_ID)
Byte 0:    101 + boardIndex (board identifier)
Byte 1:    0x01 (BU_RESPONSE_FRAME_TYPE - current gains)
Bytes 2-37: 18 x uint16_t current gains (big-endian)
Bytes 38-63: unused (zeros)
```
3ms delay after transmission.

**Frame 2 - kW Gains:**
```
CAN ID: boardIndex + 11 (FIRST_BU_ID)
Byte 0:    101 + boardIndex (board identifier)
Byte 1:    0x02 (BU_KW_FRAME_TYPE - kW gains)
Bytes 2-37: 18 x uint16_t kW gains (big-endian)
Bytes 38-63: unused (zeros)
```

### sendBoardCalibrationDataWithRetry(boardIndex)

Retry parameters:
- Max attempts: **3**
- Timeout per attempt: **2 seconds** (`startRetryTimer()`)
- Delay between retries: **10ms**

Flow per board:
```
Initialize: currentGainAck=false, kwGainAck=false, retryCount=0

LOOP (while retryCount < 3 AND !transmissionComplete):
    → sendBoardCalibrationData(boardIndex)   // Send both frames
    → retryCount++
    → startRetryTimer()                      // 2-second timeout
    → WAIT for (retryTimerExpired OR transmissionComplete)
    → stopRetryTimer()
    → If transmissionComplete → return true
    → DEVICE_DELAY_US(10000)                 // 10ms before retry

Return transmissionComplete
```

### BU Board ACK Detection (in ISR)
When BU board acknowledges, it sends back:
```
data[0] = 101 + boardIndex
data[1] = 1 (current gain ACK)
data[2] = 1 (kW gain ACK)
```
ISR detects `data[1]==1 && data[2]==1` → calls `handleBUAcknowledgment()` →
`processBUAcknowledgment()` updates `boardAckStatus[boardIndex]`.

### sendAllBoardsCalibrationDataWithRetry()
Iterates through all 12 board slots. For each active board (bit set in
`activeBUMask`), calls `sendBoardCalibrationDataWithRetry()` with 1ms
delay between boards.

---

## 10. Flash Operations

**File: `flash_driver.c/h`**

### Flash API Initialization
```c
FlashAPI_init()  // Called in startup()
→ Flash_initModule(FLASH0CTRL_BASE, FLASH0ECC_BASE, 3)
→ Fapi_initializeAPI(FlashTech_CPU0_BASE_ADDRESS, 150)  // 150 MHz
→ Fapi_setActiveFlashBank(Fapi_FlashBank0)
```
Runs from RAM (`.TI.ramfunc` pragma).

### Sector Erase
```c
Example_EraseSector()  // Erases sector at addressToWrite
→ ClearFSMStatus()
→ Fapi_setupBankSectorEnable(CMDWEPROTA, 0x00000000)   // Sectors 0-31
→ Fapi_setupBankSectorEnable(CMDWEPROTB, 0xFFFFFFFF)   // Sectors 32-127
→ Fapi_issueAsyncCommandWithAddress(Fapi_EraseSector, addressToWrite)
→ Wait FSM ready
→ Verify blank check
```

### Bank Erase
```c
eraseBank4()
→ Erase entire Bank 4 (FlashBank4StartAddress)
→ Verify in two passes:
   Pass 1: First 8 sectors (16 KB)
   Pass 2: Remaining 80 sectors (160 KB)
```

### Flash Programming
```c
Example_ProgramUsingAutoECC()
→ Loop through data in 8-word (16-byte) bursts:
   → ClearFSMStatus()
   → Setup sector protection
   → Fapi_issueProgrammingCommand(address, Buffer+i, 8, 0, 0, Fapi_AutoEccGeneration)
   → Wait FSM ready
   → Verify FMSTAT == 3
   → Fapi_doVerify() - read-back check
   → Re-enable sector protection
```

### Flash Write Enable (Password Protected)
```c
enableFlashWrite()
```
Compares received CAN data against expected password:
```
Byte 0:  0x02
Bytes 1-14: "Delta@PDUGEN3.0" (ASCII)
Bytes 15-63: 0x00
```
If password matches → `flashWriteEnabled = true` → sends `flashWriteEnabledAck()`.

---

## 11. CRC Validation

**File: `pdu_manager.c`**

### Algorithm: CRC-16 (polynomial 0x8005)

```c
uint16_t calculateCRC(uint16_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;          // Initial value
    for (i = 0; i < length; i++) {
        crc ^= data[i];
        for (j = 0; j < 16; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x8005;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

### Usage
- **On write:** CRC calculated over 451 words of PDUData, stored in `CalibrationData.crc`
- **On receive:** `validateDataCRC()` calculates CRC over `receivedBuffer[0..450]`,
  compares against `receivedBuffer[451]`
- **Constants:** `calibrationdDataSize = 451`, `CRC16_POLYNOMIAL = 0x8005`

---

## 12. Timer System

**File: `timer_driver.c/h`**

### Hardware: CPU Timer 0
- Clock: 150 MHz SYSCLK
- Period: 150,000,000 cycles = **1 second per tick**
- ISR: `INT_Cputimer_ISR()` calls `timerSystemTick()`

### Software Timers

| Timer ID                    | Enum Value | Timeout  | Auto-reload | Purpose                     |
|-----------------------------|------------|----------|-------------|------------------------------|
| `TIMER_FRAME_TRACKING`      | 0          | 2000 ms  | No          | Frame retransmission timeout |
| `TIMER_10_SEC`              | 1          | 10000 ms | No          | Discovery phase timeout      |
| `TIMER_BU_DATA_COLLECTION`  | 2          | 10000 ms | No          | BU data collection timeout   |
| `TIMER_RETRY`               | 3          | 2000 ms  | No          | Calibration send retry       |

### Global Flags (updated every tick by `timerSystemTick()`)

| Flag                    | Source Timer              | Usage                       |
|-------------------------|---------------------------|-----------------------------|
| `timer10SecExpired`     | `TIMER_10_SEC`            | Discovery phase ended       |
| `timerExpired`          | `TIMER_FRAME_TRACKING`    | Frame tracking timeout      |
| `requestRetransmission` | `TIMER_FRAME_TRACKING`    | Trigger retransmission      |
| `retryTimerExpired`     | `TIMER_RETRY`             | BU calibration send timeout |
| `buDataTimeoutOccurred` | `TIMER_BU_DATA_COLLECTION`| BU data collection timeout  |

### Tick Rate vs Timer Values
With `TIMER_TICK_TIME_MS = 1000` (1 second per tick):
- 10000 ms timer = 10 ticks = 10 seconds
- 2000 ms timer = 2 ticks = 2 seconds

---

## 13. ISR Message Routing

**File: `isr.c`**

### MCANIntr1ISR() - Single ISR for both MCANA and MCANB

```
Interrupt fires
    |
    ├── Clear interrupt status (both MCANA and MCANB)
    |
    ├── TX Completion (MCAN_IR_TC_MASK)
    │   → Clears pending flag so getTxBufReqPend() returns 0
    │   → Prevents hang in blocking TX wait loops
    |
    ├── MCANA RX FIFO 0 (MCAN_IR_RF0N_MASK)
    │   → Read FIFO messages into rxFIFO0Msg[]
    │   → processReceivedFrame(order)  // M-Board calibration frame tracking
    │   → bufferFull = true when getIndex == 14
    |
    ├── MCANB RX FIFO 0 (MCAN_IR_RF0N_MASK)
    │   → Same as MCANA FIFO processing
    |
    ├── MCANA Dedicated Buffer (MCAN_IR_DRX_MASK)
    │   → For each buffer with new data:
    │     ├── boardId in [11,22] (BU board):
    │     │   → buMessagePending[i] = true
    │     │   → buMessageReceived = true
    │     │   → processBURuntimeFrame()    // Immediate runtime copy
    │     │   → runtimeMessageReceived = true
    │     ├── boardId == 3 (M-Board):
    │     │   → CommandReceived = true
    │     └── data[1]==1 && data[2]==1 (ACK):
    │         → handleBUAcknowledgment()
    |
    └── MCANB Dedicated Buffer (MCAN_IR_DRX_MASK)
        → Same routing logic as MCANA buffers
```

### BU Message Processing in Main Loop
```c
processBUMessages()  // Called when buMessageReceived == true
→ For each buMessagePending[i]:
    → Extract boardId and frameType from rxMsg[i]
    → If discoveryInProgress:
        → processBUDiscoveryResponse(boardId)  // Discovery mode
    → Else if frameType == 0x01 or 0x02:
        → processBUFrameInISR(&rxMsg[i])       // Data collection mode
    → buMessagePending[i] = false
```

---

## 14. Main Loop State Machine

**File: `adc_ex2_soc_epwm.c`**

### Loop Execution Order (every iteration)

```
while(1) {
    z++;

    // 1. Process BU runtime messages
    if (runtimeMessageReceived) → processBURuntimeMessages()

    // 2. Process BU board messages (discovery + calibration data)
    if (buMessageReceived) → processBUMessages()

    // 3. Handle frame retransmission requests
    if (requestRetransmission) → handleMissingFrames()

    // 4. Process CAN RX FIFO
    if (bufferFull) → copyRxFIFO()

    // 5. Check BU data collection completion
    if (activeBUMask != 0 &&
        receivedCurrentFrames == activeBUMask &&
        receivedKWFrames == activeBUMask)
        → buDataCollectionComplete = true
        → syncBUBoardsData()
        → sendToMBoard = true

    // 6. Check discovery completion
    if (discoveryInProgress && timer10SecExpired)
        → finalizeBUDiscovery()
        → copyFlashToBUBoards()  (if activeBUMask != 0)
        → Set all flags: discoveryComplete, buDataCollectionComplete,
                         buDataProcessed, sendToMBoard

    // 7. Send calibration to M-Board
    if (discoveryComplete && buDataCollectionComplete &&
        buDataProcessed && sendToMBoard)
        → sendCalibrationDataToMBoard()

    // 8. Handle M-Board commands
    if (CommandReceived) → switch(rxMsg[3].data[0]) { ... }

    // 9. Metrology calculations
    App_calculateMetrologyParameters()

    // 10. Voltage capture (DMA + ePWM + ADC)
    // 11. DAC playback (blocking wait)
    // 12. Send voltages to BU boards
    // 13. Reset frame tracking
    // 14. Send runtime params to M-Board
    // 15. GPIO29 heartbeat toggle
}
```

### Command Handler Detail

| Command | Action |
|---------|--------|
| `0x05` CMD_START_DISCOVERY | `sendDiscoveryStartAck()` + `startNewDiscovery()` |
| `0x03` CMD_READ | `findReadAndWriteAddress()` + `prepareFramesForTransmission()` + `handleRetransmissionRequest()` |
| `0x01` CMD_VERSION_CHECK | `sendCalibrationDataFormatVersion()` |
| `0x02` CMD_FLASH_ENABLE | `enableFlashWrite()` (password check) |
| `0x09` CMD_ERASE_BANK4 | `eraseBank4()` + `erasedBank4Ack()` |
| `0x0A` CMD_WRITE_DEFAULT | `findReadAndWriteAddress()` + `eraseSectorOrFindEmptySector()` + `writeDefaultCalibrationValues()` + `Example_ProgramUsingAutoECC()` + `writtenDefaultCalibrationValuesAck()` |
| `0xFD` _(runtime)_ | `processSBoardRuntimeToMBoard()` |

After every command: `syncReadWriteToWorking()` if `!updateFailed`.

---

## 15. Acknowledgment Frames

All ACKs are 64-byte CAN-FD frames. Only the first few bytes carry data.

### ACKs Sent on MCANB (to M-Board), CAN ID 0x4

| Event                   | Byte 0 | Byte 1 | Byte 2 | Notes |
|-------------------------|--------|--------|--------|-------|
| Discovery Start ACK     | 0x05   | 0x01   | -      | Echo + OK |
| Version Check           | 0x01   | major  | minor  | Version 1.0 |
| Flash Write Enabled     | 0x02   | 0x01   | -      | Password accepted |
| Flash Write Disabled    | 0x02   | 0x00   | -      | Password rejected |
| Bank 4 Erased           | 0x09   | 0x01   | -      | Erase complete |
| Default Values Written  | 0x0A   | 0x01   | -      | Defaults programmed |
| Calib Update Success    | 0x04   | 0x01   | -      | Data written OK |
| Calib Update Fail       | 0x04   | 0x00   | -      | Data write failed |

### ACKs Sent on MCANA (to BU Boards), CAN ID 0x4

| Event                   | Byte 0 | Byte 1              | Byte 2   |
|-------------------------|--------|----------------------|----------|
| Discovery Response ACK  | 0x0D   | 0x64+boardId-0x5     | 0x01     |
| BU Frame ACK            | 100+boardId-5 | currentReceived | kwReceived |
| Duplicate ID Error      | 0x0F   | boardID              | -        |

---

## 16. Default Values

**File: `pdu_manager.c` — `writeDefaultCalibrationValues()`**

### Gains (all 216 channels)
| Parameter | Default | Notes |
|-----------|---------|-------|
| ctGain    | 885     | Current transformer gain |
| kwGain    | 9677    | Power gain               |

### Primary Measurements
| Parameter       | R    | Y    | B    |
|-----------------|------|------|------|
| Voltage         | 3770 | 3770 | 3770 |
| Current         | 3300 | 3300 | 3300 |
| Neutral Current | 2000 | -    | -    |
| Total kW        | 165  | -    | -    |

### Secondary Measurements
| Parameter       | R    | Y    | B    |
|-----------------|------|------|------|
| Voltage         | 2405 | 2345 | 2392 |
| Current         | 1855 | 1865 | 1865 |
| Neutral Current | 1905 | -    | -    |
| kW              | 3950 | 4100 | 4100 |

### Other
| Parameter      | Default |
|----------------|---------|
| Ground Current | 100     |

---

## 17. State Variables Reference

### Discovery State (`bu_board.c/h`)
| Variable               | Type      | Purpose                                |
|------------------------|-----------|----------------------------------------|
| `discoveryInProgress`  | bool      | True during 10-second discovery window |
| `discoveryComplete`    | bool      | True after finalizeBUDiscovery()       |
| `discoveryMask`        | uint32_t  | Bitmask of boards found in current discovery |
| `activeBUMask`         | uint32_t  | Cumulative bitmask of all active boards |
| `numActiveBUBoards`    | uint8_t   | Count of active boards (0-12)          |
| `duplicateIDFound`     | bool      | Duplicate board ID detected            |
| `acknowledgedBoardCount`| uint32_t | Number of acknowledged boards          |

### BU Data Collection (`bu_board.c/h`)
| Variable                  | Type      | Purpose                              |
|---------------------------|-----------|--------------------------------------|
| `buDataCollectionActive`  | bool      | Data collection phase active         |
| `buDataCollectionComplete`| bool      | All expected frames received         |
| `buDataProcessed`         | bool      | Data synced to pduManager            |
| `receivedCurrentFrames`   | uint32_t  | Bitmask of current gain frames RX'd  |
| `receivedKWFrames`        | uint32_t  | Bitmask of kW gain frames RX'd      |
| `buMessageReceived`       | bool      | ISR flag: BU CAN message arrived     |
| `buMessagePending[19]`    | bool[]    | Per-buffer pending flag              |

### M-Board Communication (`m_board.c/h`, `can_driver.c`)
| Variable                | Type      | Purpose                               |
|-------------------------|-----------|---------------------------------------|
| `CommandReceived`       | bool      | ISR flag: M-Board command arrived     |
| `sendToMBoard`          | bool      | Ready to send calibration to M-Board  |
| `sendToBUBoard`         | bool      | Ready to send calibration to BU boards|
| `calibDataTransmissionInProgress` | bool | TX in progress               |
| `calibDataTransmissionComplete`   | bool | TX finished                  |

### Flash State (`flash_driver.c/h`)
| Variable            | Type      | Purpose                               |
|---------------------|-----------|---------------------------------------|
| `newestData`        | uint16_t  | Sequence number of newest flash sector|
| `oldestData`        | uint16_t  | Sequence number of oldest flash sector|
| `readAddress`       | uint32_t  | Flash address of newest data          |
| `writeAddress`      | uint32_t  | Flash address of oldest data          |
| `addressToWrite`    | uint32_t  | Current write target address          |
| `emptySectorFound`  | bool      | Empty sector available in Bank 4      |
| `flashWriteEnabled` | bool      | Flash write unlocked (password OK)    |
| `updateFailed`      | bool      | Last flash operation failed           |

### Timer Flags (`timer_driver.c/h`)
| Variable               | Type | Purpose                             |
|------------------------|------|-------------------------------------|
| `timer10SecExpired`    | bool | Discovery 10-second timer expired   |
| `timerExpired`         | bool | Frame tracking timer expired        |
| `requestRetransmission`| bool | Frame retransmission needed         |
| `retryTimerExpired`    | bool | BU calibration retry timer expired  |
| `buDataTimeoutOccurred`| bool | BU data collection timed out        |

---

## 18. File Reference

| File | Purpose |
|------|---------|
| `common.h` | Command codes, CAN config, constants, structure forward declarations |
| `pdu_manager.h/c` | Data structures, CRC, frame preparation, default values |
| `flash_driver.h/c` | Flash read/write/erase, wear-leveling, password protection |
| `bu_board.h/c` | BU discovery, data collection, calibration send with retry |
| `m_board.h/c` | M-Board comms, version check, retransmission, calibration TX |
| `can_driver.h/c` | CAN filter setup, message routing, retransmission logic |
| `isr.c` | ISR message routing for MCANA/MCANB RX/TX |
| `timer_driver.h/c` | Software timer system (discovery, retry, frame tracking) |
| `adc_ex2_soc_epwm.c` | Main loop, command handler, state machine |
