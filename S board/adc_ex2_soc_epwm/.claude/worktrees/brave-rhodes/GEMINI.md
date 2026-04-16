# Project Overview: PDU Firmware (S-Board)

This project is the firmware for the **S-Board (Sensor Board)** within a Power Distribution Unit (PDU) system. It runs on the **Texas Instruments C2000 F28P55x** microcontroller.

The firmware is responsible for:
1.  **Metrology:** Measuring voltages, currents, and power (Active/Reactive) using the on-chip ADC and a metrology library.
2.  **Communication:** Interfacing with a Main Board ("M-Board") and multiple Battery Unit Boards ("BU-Boards") via **MCAN** (CAN FD).
3.  **Control:** Synchronizing measurements using ePWM and processing data in real-time.
4.  **Data Management:** Storing and retrieving calibration data from the internal Flash memory (Bank 4).

## Architecture

*   **Device:** TI TMS320F28P55x (configured as F28P55x_128PDT).
*   **IDE/Toolchain:** Code Composer Studio (CCS) / TI C2000Ware.
*   **Configuration:** Uses **SysConfig** (`.syscfg`) for peripheral setup (ADC, ePWM, GPIO, Timer).

### Key Subsystems

*   **Main Loop (`adc_ex2_soc_epwm.c`):** Orchestrates the system `startup()`, handles the background loop, processes CAN commands, and manages state (Discovery, Data Collection, Transmission).
*   **Metrology (`metro.h`, `metrology/`):** Handles signal processing. `cpuTimer1ISR` triggers per-sample processing (`Metrology_perSampleProcessing`).
*   **PDU Manager (`pdu_manager.c/h`):** Manages data structures for Primary/Secondary measurements, Calibration Gains, and Flash persistence.
*   **Communication (`can_driver.c`, `s_board_*`):**
    *   **M-Board Link:** Receives commands (Read, Write, Erase, Discovery) and sends aggregated data.
    *   **BU-Board Link:** Discovers connected BU boards and collects their voltage/current/power data.
*   **Flash Storage:** Saves calibration data (gains) to Flash Bank 4 to persist across resets.

## Key Files

*   `adc_ex2_soc_epwm.c`: **Entry point**. Contains `main()`, `startup()`, `EPWM_init()`, and the main command processing loop.
*   `adc_ex2_soc_epwm.syscfg`: **Hardware Configuration**. Defines PinMux, ADC triggers, ePWM settings, and Timer periods.
*   `common.h`: **Global Definitions**. CAN message IDs, Flash addresses, Command codes, and shared constants.
*   `pdu_manager.h`: **Data Structures**. Defines `PDUData`, `CalibrationData`, and `CalibrationGains`.
*   `metro.h`: **Metrology Interface**. exposing `gMetrologyWorkingData` and processing functions.
*   `bu_board.h` / `m_board.h`: Abstractions for interacting with other boards in the system.

## Build & Run

This project is a CCS project.

1.  **Import:** Import the project into Code Composer Studio.
2.  **SysConfig:** Ensure the correct C2000Ware version is linked for SysConfig generation.
3.  **Build:** Use the standard CCS Build (Hammer icon).
    *   Configurations: `CPU1_FLASH` (for deployment) or `CPU1_RAM` (for debugging).
4.  **Debug:** Launch using the `targetConfigs/TMS320F28P550SJ9_LaunchPad.ccxml` (or similar) target configuration.

## Development Conventions

*   **Naming:** TI DriverLib style (e.g., `GPIO_setPinConfig`, `EPWM_setClockPrescaler`).
*   **Interrupts:** ISRs are defined in `adc_ex2_soc_epwm.c` and mapped in SysConfig. `cpuTimer1ISR` is the high-frequency metrology tick.
*   **Memory:** Large arrays (like metrology buffers) are placed in specific sections (e.g., `.bss:large_arrays`).
*   **Safety:** Flash operations (Write/Erase) are guarded and synced with working RAM structures.
