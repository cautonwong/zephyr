# GEMINI.md - Application Workspace (Industrial Grade)

This document defines the architectural mandates, engineering standards, and development workflows for the `/workspaces/applications` workspace. This is an industrial-grade Zephyr RTOS project focusing on high reliability, strict hardware decoupling, and sophisticated multi-core orchestration.

## 1. Project Context & Identity
- **Architecture**: Application-centric West T2 topology.
- **Focus**: Decoupled, multi-platform firmware with advanced IPC (M0/M33).
- **Quality Tier**: Industrial Grade (Mandatory HIL testing, Trace-based profiling, Coredump readiness).

## 2. Architectural Mandates

### 2.1 Strict Hardware Decoupling (DTS-First)
- **Zero Hardcoding**: No hardware-specific defines (`GPIOA`, `PIN_5`, etc.) allowed in application code (`apps/`).
- **Alias Strategy**: All hardware resources (LEDs, buttons, sensors, UARTs) must be accessed via Devicetree aliases defined in board DTS or application overlays.
- **Node Completeness**: Every SOC peripheral (UART, SPI, I2C, ADC) must be defined in the SOC `.dtsi`, even if disabled. Apps enable them via overlays.

### 2.2 IPC & Multi-Core Orchestration
- **Standardized IPC**: Use Zephyr's `IPM` (Inter-Processor Messaging) and `OpenAMP` (RPMsg) for data exchange between cores (e.g., V32F20x M0 and M33).
- **Memory Safety**: Direct raw pointer access to Shared Memory (SHM) is strictly forbidden. Use RPMsg APIs or the `ipc_service` framework.
- **Functional Partitioning**:
    - **Real-Time Core (M0)**: Dedicated to IO, high-frequency interrupts, and metering.
    - **Gateway Core (M33)**: Dedicated to protocol stacks, storage (FlashDB), and connectivity.

### 2.3 Modular Pluggability
- **Kconfig-Driven**: All business logic and optional features must be guarded by Kconfig symbols.
- **Conditional Compilation**: Use `target_sources_ifdef` in `CMakeLists.txt` to keep binary sizes optimized and logic decoupled.

## 3. Engineering Standards

### 3.1 Driver Quality
- **Standard APIs**: Drivers must implement established Zephyr APIs (`sensor_driver_api`, `uart_driver_api`, etc.).
- **HAL Decoupling**: SOC drivers (in `modules/soc/`) must use the vendor HAL (in `modules/hal/`) without introducing app-level dependencies.
- **State Management**: Drivers should be stateless where possible or use clear instance data for multi-instance support.

### 3.2 C Coding Standards
- **Defensive Programming**: Validate all inputs; handle all error codes from Zephyr kernel/driver APIs.
- **No Blocking in ISR**: Interrupt handlers must be extremely lean. Offload work to workqueues or threads.

## 4. Full-Cycle Quality Loop (The Engineering Flywheel)
The project adheres to a strict continuous quality loop. No code is considered "complete" until it passes all gates of the flywheel.

### 4.1 Step 1: Local Logic Verification (native_sim)
Before touching hardware, verify business logic on Linux.
- **ASAN (Address Sanitizer)**: Mandatory for memory safety checks.
  ```bash
  west twister -T tests/ -p native_sim --enable-asan
  ```
- **Mocking (FFF)**: Use FFF to stub hardware drivers in `tests/` to isolate application logic.

### 4.2 Step 2: System Interaction Testing (Pytest)
For complex flows (e.g., protocol handshakes, OTA), use Pytest to drive the shell and verify state machines.
- **Location**: Found in `tests/**/pytest/`.
- **Logic**: Python scripts send commands and parse regex-based responses to confirm system state.

### 4.3 Step 3: Automated HIL Testing (Twister Hardware)
Final validation on physical SOCs.
- **Mandatory HIL**: All driver changes must pass HIL.
- **Fixtures**: Some tests require physical loopbacks (e.g., `fixtures: [rs485_loopback]`). Twister will skip these unless a matching hardware-map entry is provided.
- **Command**:
  ```bash
  west twister -T tests/ --device-testing --hardware-map hardware_map.yaml
  ```

### 4.4 Step 4: High-Performance Test Loop (Fast-Sync)
To minimize disk I/O bottlenecks during massive test runs, the `workflow.sh` script can mount tests to RAM-Disk (`/root/fast_space`).
- **Command**: `./workflow.sh test <tag>`

### 4.5 Build Workflow (Professional Manager)
The project uses `workflow.sh` as the primary entry point for building. This script automates high-performance syncing to fast storage (`/root/fast_space`) and dynamically configures `ZEPHYR_MODULES` paths.
...

**Mandatory Usage:** Always build through `workflow.sh` to ensure correct HAL/SOC mapping.

- **Build for Hardware:**
  ```bash
  ./workflow.sh build <app_name> <target_profile>
  ```
  *Example:* `./workflow.sh build vango_demo v32f20x_target`

- **Native Linux Simulation:**
  Used for rapid logic verification without physical hardware.
  ```bash
  ./workflow.sh sim <app_name>
  ```

- **Cleaning Builds:**
  ```bash
  ./workflow.sh clean <app_name>
  ```

### 4.3 SOC Target Profiles
The workflow manager distinguishes between SOC families and maps the corresponding HAL/SOC modules automatically:
- **v32f20x**: Targets `V32F20X_StdPeriph_Lib_V1.0.6`.
- **v85xxp**: Targets `V85XXP_Lib_V2.5`.

Ensure your `<target_profile>` name reflects the intended SOC to trigger correct module injection.

## 5. Debugging & Observability

### 5.1 System Profiling (Tracing)
- **God's Eye View**: Use `SEGGER SystemView` for real-time profiling of thread scheduling and ISR latency.
- **Requirement**: Keep `CONFIG_TRACING=y` and `CONFIG_SEGGER_SYSTEMVIEW=y` enabled during performance tuning.

### 5.2 Post-Mortem Analysis (Coredump)
- **Crash Recovery**: Coredump is required for production-ready builds.
- **Analysis**: Use `scripts/coredump/coredump_serial_log_parser.py` to extract and analyze crash logs with GDB.

## 6. Technical Environment
- **Toolchain**: Zephyr SDK 1.0.1 located at `/opt/zephyr-sdk-1.0.1/`.
- **Python Env**: Activate the project venv before running west commands:
  ```bash
  source /workspaces/edgeos/EdgeC/.venv/bin/activate
  ```
- **Manifest**: Managed via `west.yml` in the root of this directory.

---
*Note: This GEMINI.md takes precedence over general defaults for all development in /workspaces/applications.*
