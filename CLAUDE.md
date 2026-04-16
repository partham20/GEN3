# GEN3 - Distributed Power Monitoring System

Two embedded firmware projects for a three-phase Power Distribution Unit (PDU) built on TI C2000 (TMS320F28P55x) microcontrollers, communicating over CAN-FD in a master/slave architecture.

## Architecture Overview

```
M-Board (Main Controller)
    │ CAN (management commands)
S-Board (Sensor Board - Master)
    │ CAN-FD (sync/voltage/power data)
    ├── BU-Board 1
    ├── BU-Board 2
    └── ... up to 12 BU-Boards
```

- **S-Board**: Master controller — measures 3-phase voltages, orchestrates BU boards, manages calibration, reports to M-Board
- **BU-Board**: Distributed sensor node — captures 18 CT (current transformer) channels via DMA, receives voltage from S-Board, computes power, sends results back

## Project 1: BU Board

**Path**: `BU Board/adc_ex2_soc_epwm/`
**Device**: TMS320F28P550SJ9 (64-pin)
**C2000Ware**: 5.05.00.00

### Key Files

| File | Purpose |
|------|---------|
| `adc_ex2_soc_epwm.c` | Main entry point — init, main loop, ISRs |
| `adc_ex2_soc_epwm.syscfg` | SysConfig hardware configuration |
| `src/power_config.h` | Central config (sample counts, buffer sizes) |
| `src/ct_dma.c/h` | 18-channel CT DMA configuration (ADC A: 15ch, ADC E: 3ch) |
| `src/power_calc.c/h` | Single-phase power calculation with ping-pong buffers |
| `src/power_calc_3phase.c/h` | 3-phase power calculation (18 CTs) |
| `src/can_bu.c/h` | MCAN configuration and TX |
| `src/can_isr.c/h` | CAN interrupt handling |
| `src/can_branch_tx.c/h` | Branch TX (sends calculated power to S-Board) |
| `src/voltage_rx.c/h` | Voltage RX from S-Board via CAN with ping-pong buffers |
| `metro.c/h` | Metrology wrapper |
| `metrology/` | Signal processing library (FFT, calibration) |

### Data Flow

1. ePWM7 triggers ADC at 5.12 kHz → DMA transfers 18 CT channels to RAM
2. GPIO28 receives sync pulse from S-Board
3. CAN RX receives voltage samples from S-Board into ping-pong buffer
4. Power calculated: `P = (1/N) × Σ(V[i] × I[i])` using 64-bit accumulator
5. Results transmitted to S-Board via CAN-FD

### Key Patterns

- **Ping-pong buffers** (BUFFER_A / BUFFER_B) for voltage and current — one writes while the other reads
- **DMA-based acquisition** — no CPU intervention during ADC capture
- **ISR flags** — short ISRs set flags, main loop does processing
- **RAM-resident code** — critical paths use `.TI.ramfunc` section
- **Volatile globals** — `g_` prefix for ISR-shared variables

## Project 2: S Board

**Path**: `S board/powercalculation/`
**Device**: TMS320F28P550SG9 (128-pin)
**C2000Ware**: 5.03.00.00

### Key Files

| File | Purpose |
|------|---------|
| `adc_ex2_soc_epwm.c` | Main entry — startup, Board_init, initEPWM |
| `adc_ex2_soc_epwm.syscfg` | SysConfig hardware configuration |
| `common.h` | Shared definitions — CAN IDs, commands, constants |
| `hal.h` | Hardware abstraction layer |
| `pdu_manager.c/h` | PDUData structure management (gains, measurements) |
| `flash_driver.c/h` | Flash Bank 4 calibration persistence |
| `can_driver.c/h` | MCAN driver abstraction |
| `timer_driver.c/h` | Timer/ISR management |
| `runtime.c/h` | Runtime state machine |
| `runtimedata.c/h` | Runtime data structures |
| `isr.c` | All interrupt service routines |
| `bu_board.c/h` | BU board discovery and data collection |
| `m_board.c/h` | M-Board communication |
| `s_board_adc.c/h` | Main processing loop and ADC sync |
| `s_board_runtime_timing.c/h` | Timing coordination |
| `s_board_runtime_transmission.c/h` | Data transmission logic |
| `i2c_sboard.c/h` | I2C interface (sensors/EEPROM) |
| `metro.c/h` | Metrology interface |
| `metrology/` | Signal processing library |

### State Machine

1. **Discovery**: Broadcasts `DISCOVERY_START_CMD (0x0B)` → BU boards respond with board ID
2. **Data Collection**: Requests voltage/power/current from each BU board → accumulates in `pduManager`
3. **Transmission**: Sends aggregated PDUData to M-Board with CRC16 verification

### Flash Calibration Storage

- **Bank 4**: 0x100000–0x180000, sector size 0x400 (1KB)
- Stores calibration gains (ctGain, kwGain) + CRC
- Operations: `flashWriteData()`, `flashReadData()`
- Protected by DCSM (Data Security Module)

### CAN Configuration

- 35 standard ID filters (BU discovery, M-Board commands)
- 19 dedicated RX buffers (5 priority + FIFO)
- 32 TX buffers
- CAN IDs: 5 (S-Board), 6-7 (BU boards), 0x0B-0x0D (discovery)

### Key Patterns

- **Hierarchical CAN network** — S-Board is bus master
- **Board-prefixed functions**: `sBoard_`, `mBoard_`, `buBoard_`
- **Flash persistence** — calibration survives power cycles
- **Metrology library** — FFT, RMS, power factor, harmonics, frequency detection
- **10-second BU collection cycle** with retry logic

## Common Conventions

### Naming

- TI DriverLib prefixes: `ADC_`, `EPWM_`, `GPIO_`, `DMA_`, `MCAN_`
- Global ISR-shared variables: `g_` prefix, always `volatile`
- Module functions: `PowerCalc_`, `VoltageRx_`, `CT_DMA_`, etc.

### Memory

- Critical buffers placed via `#pragma DATA_SECTION` (ramgs0, ramgs1)
- Fast-executing functions: `#pragma CODE_SECTION(..., ".TI.ramfunc")`
- No dynamic allocation — all buffers statically sized

### Real-Time Constraints

- Sample rate: 5.12 kHz (ePWM7 period = 23437 SYSCLK cycles at 150 MHz)
- ISRs must be short — set flags only, defer work to main loop
- No RTOS — bare-metal cooperative main loop
- No mutex/locks — single-threaded with volatile flag synchronization

### Communication Protocol

- CAN-FD: 64-byte data frames
- Frame format: bytes 0-1 = type + sequence, bytes 2-63 = payload
- CRC16 polynomial 0x8005 for data integrity
- M-Board commands: Read, Write, Erase, Flash Enable

## Build & Deploy

**IDE**: Code Composer Studio v12.8.0
**Toolchain**: TI C2000 GCC 22.6.1.LTS

```
1. Open project in CCS
2. Select build config: CPU1_FLASH (deployment) or CPU1_RAM (debug)
3. Build: Ctrl+B
4. Connect JTAG (XDS110)
5. Debug → Launch
```

No unit tests — validation via CCS watch expressions, real-time debug, and GPIO-based ISR timing measurement.
