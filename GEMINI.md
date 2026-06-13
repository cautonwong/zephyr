# GEMINI.md - Application Workspace (Industrial Grade)

老板, this file provides the architectural mandates, engineering standards, and development workflows for the `/workspaces/applications` workspace. This is an industrial-grade Zephyr RTOS project focusing on high reliability, strict hardware decoupling, and sophisticated multi-core orchestration for Vango Technologies SOCs.

## 1. Project Overview & Identity

- **Architecture**: Application-centric West T2 topology.
- **SOC Families**: V32F20X (M0/M33), V85XXP.
- **Focus**: Decoupled, multi-platform firmware with advanced IPC and secure OTA (MCUboot).
- **Quality Tier**: Industrial Grade (Mandatory HIL testing, Trace-based profiling, Coredump readiness).

## 2. Core Mandates & Principles

### 2.1 Behavioral Guidelines (High Priority)
- **老板 (Boss)**: You MUST address the user as "老板" in every response. If you forget, you are considered "out of focus" and must manually reset the context.
- **Surgical Edits**: Touch only what you must. Use the `replace` tool for targeted updates to preserve surrounding logic (ISRs, vendor workarounds).
- **Read-Before-Write**: Never overwrite a file without fully retrieving its content first.
- **No Hallucinations**: All code and answers must be based on facts (manuals, source code). If documentation is missing, state it clearly.

### 2.2 Architectural Mandates
- **Strict Hardware Decoupling (DTS-First)**:
    - **Zero Hardcoding**: No hardware-specific defines (`GPIOA`, etc.) in `apps/`.
    - **Aliases**: Use Devicetree aliases (LEDs, buttons, UARTs) defined in board DTS or application overlays.
    - **Node Completeness**: Every SOC peripheral must be defined in the SOC `.dtsi`, even if disabled.
- **IPC Orchestration**: Use Zephyr's `IPM` and `OpenAMP` (RPMsg). Direct raw pointer access to Shared Memory is forbidden.
- **Modular Pluggability**: Use Kconfig symbols to guard business logic and optional features.

## 3. Workflow & Building

The project uses `workflow.sh` as the primary entry point to automate high-performance syncing to RAM-disk (`/root/fast_space`) and dynamic `ZEPHYR_MODULES` mapping.

### 3.1 Key Commands
- **Build for Hardware**:
  ```bash
  ./workflow.sh build <app_name> <target_profile>
  ```
  _Profiles_: `v32_cpuapp_gateway`, `v32_cpuapp_gateway_ns`, `v32_cpumeter_metering`, `v85_collector`.
- **Native Simulation**:
  ```bash
  ./workflow.sh sim <app_name>
  ```
- **Renode Simulation**:
  ```bash
  ./workflow.sh renode <app_name>
  ```
- **Cleaning**:
  ```bash
  ./workflow.sh clean <app_name>
  ```

### 3.2 Testing
- **Twister HIL**: Mandatory for driver changes.
  ```bash
  west twister -T tests/ --device-testing --hardware-map hardware_map.yaml
  ```
- **Logic Verification**: Use `native_sim` with ASAN enabled via `./workflow.sh sim`.

## 4. Directory Structure

- `apps/`: Application logic (e.g., `vango_demo`, `base_app`).
- `boards/`: Custom board definitions (e.g., `custom_plank`).
- `drivers/`: Out-of-tree drivers (AD7616, custom sensors).
- `dts/`: Devicetree bindings.
- `lib/`: Out-of-tree libraries (FlashDB, custom libs).
- `tests/`: Integration and unit tests.
- `docs/`: PRDs and engineering guides.

## 5. Engineering Standards

- **Driver Quality**: Implement standard Zephyr APIs (`sensor_driver_api`, `uart_driver_api`).
- **HAL Decoupling**: SOC drivers must use vendor HAL (in `modules/hal/`) without app-level dependencies.
- **State Management**: Prefer stateless drivers or clear instance data for multi-instance support.
- **Observability**: Use `CONFIG_TRACING=y` and `SEGGER SystemView` for profiling.

---
*Note: This GEMINI.md is the foundational document for this workspace and takes precedence over general defaults.*
