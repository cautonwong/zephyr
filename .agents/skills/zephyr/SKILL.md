---
name: zephyr
description: Use when developing, adapting, or building applications, custom board overlays, out-of-tree drivers, and multi-image TrustZone (TF-M) setups for Vango v32f20x and v85xxp SoCs on Zephyr RTOS.
---

# Zephyr Development Guidelines

## Overview
This skill defines the development standards and architectural mandates for the Vango V32F20x and V85xxp SoCs within the Zephyr RTOS ecosystem. All firmware components must adhere to strict hardware decoupling (DTS-first), non-intrusive vendor HAL integration, and multi-image security boundaries.

---

## When to Use
- **Hardware Porting**: Creating or altering board configurations, Devicetree Source (DTS) bindings, or board pinctrl.
- **Driver Adaptations**: Writing out-of-tree drivers (e.g., UART, GPIO, WDT) wrapping standard Zephyr APIs.
- **Multi-Image Orchestration**: Integrating TrustZone-M (TF-M), MCUboot, and multi-core IPC (Cortex-M33 + Cortex-M0 cpumeter).
- **Compilation Diagnostics**: Debugging sysbuild dependencies, Kconfig violations, or link-time stack symbol faults.

---

## Core Pattern

### 1. Hardware Decoupling (DTS-First)
Never reference hardware-specific registers, memory offsets, or raw GPIO ports in application code. Extract all properties from the Devicetree using standard Zephyr node identifiers and aliases.

<Bad>
```c
/* ❌ Hardcoded registers: highly fragile and board-dependent */
#define USART0_BASE 0x40013800UL
void uart_init(void) {
    volatile uint32_t *cr1 = (uint32_t *)(USART0_BASE + 0x0C);
    *cr1 |= (1U << 13); /* Enable UART */
}
```
</Bad>

<Good>
```c
/*  DTS-First: completely decoupled via DT aliases and bindings */
#define UART_NODE DT_ALIAS(debug_uart)

#if DT_NODE_HAS_STATUS_OKAY(UART_NODE)
const struct device *const uart_dev = DEVICE_DT_GET(UART_NODE);
#else
#error "Debug UART node is disabled or missing from the Devicetree"
#endif

void uart_init(void) {
    if (!device_is_ready(uart_dev)) {
        return;
    }
    /* Configure via standard Zephyr Driver APIs */
}
```
</Good>

### 2. Non-Intrusive Integration
All upstream repositories (`rtos/zephyr/`) and vendor HALs (`modules/hal/`) must remain **100% clean and unmodified**. Implement adaptations as Zephyr Modules (`modules/soc/` or out-of-tree `drivers/`) linked dynamically through `ZEPHYR_MODULES` in `workflow.sh`.

---

## Quick Reference

### 1. Build and Run Targets
Primary build operations are managed through `./workflow.sh` to handle Ram-disk syncing and `ZEPHYR_MODULES` path stitching.

| Command | Action | Primary Profiles |
|---------|--------|------------------|
| `./workflow.sh build vango_demo <profile>` | Build for Physical HW | `v32_cpuapp_gateway_ns`, `v32_cpumeter_metering` |
| `./workflow.sh sim vango_demo` | Run Native Simulator (ASAN) | `native_sim` |
| `./workflow.sh renode vango_demo` | Run Renode Multi-Core Emulation | `v32_cpuapp_gateway` |
| `./workflow.sh clean vango_demo` | Purge Build Artifacts | All profiles |

### 2. Flash Layout Topology (2304KB Total)
When adjusting partitions, ensure absolute alignment and avoid overlapping boundaries:
- **0x000000 - 0x010000** (64KB): `boot_partition` (MCUboot)
- **0x010000 - 0x050000** (256KB): `slot0_partition` (TF-M Secure)
- **0x050000 - 0x0A0000** (320KB): `slot0_ns_partition` (vango_demo application)
- **0x0A0000 - 0x0E0000** (256KB): `slot1_partition` (TF-M Secure Upgrade Slot)
- **0x0E0000 - 0x130000** (320KB): `slot1_ns_partition` (vango_demo Upgrade Slot)
- **0x130000 - 0x134000** (16KB): `tfm_ps_partition` (Protected Storage)
- **0x134000 - 0x13C000** (32KB): `tfm_its_partition` (Internal Trusted Storage)
- **0x13C000 - 0x140000** (16KB): `tfm_nv_counters_partition` (NV Counters)
- **0x140000 - 0x150000** (64KB): `scratch_partition` (Swap Cache)
- **0x150000 - 0x170000** (128KB): `cpumeter_partition` (Cortex-M0 Image)

---

## Common Mistakes

- **Directly editing upstream RTOS code**: Always use shim definitions or override config symbols locally via prj.conf or board overlays.
- **DTS Partition Conflicts**: Overwriting default partition layouts in overlays without declaring `/delete-node/` for the original partitions leads to overlapping label errors during Devicetree compile (DTC).
- **Forgetting `git status` Locks**: Docker shared FUSE volumes can cause git extensions to lock CPU and disk I/O. Run `pkill -9 -f git` regularly to free resources.
- **Mismatched TF-M Driver Alignments**: If defining secure partitions, ensure `TFM_HAL_FLASH_PROGRAM_UNIT` matches physical sector programming constraints (e.g. `0x4` for V32F20x).
- **Relocating `sysbuild.cmake`**: Never move `sysbuild.cmake` out of the application's root directory (e.g., to a subdirectory like `sysbuild/`). Zephyr's sysbuild system expects it in the root. Moving it causes board-specific configurations to fail, defaulting to a stub board.
- **Mismatched Power Management (PM) States**: In the `pm_device_state` enum (in `pm/device.h`), intermediate states like `RESUMING` do not exist. Only use standard states: `ACTIVE`, `SUSPENDED`, `SUSPENDING`, and `OFF`.
- **Misplacing CMake Variables in `prj.conf`**: Never write CMake configuration flags (such as `KCONFIG_WERROR`) in a `prj.conf` file as a `CONFIG_*` symbol. Assign them in `CMakeLists.txt` or via the CMake CLI parameter `-DKCONFIG_WERROR=OFF`.
- **Vango IRQ Registration Violations**: Since all Vango GPIO groups share a single NVIC interrupt line (IRQ 29), do not attempt to register multiple ISRs using `LISTIFY(16)`. Use a unified single IRQ connection macro to avoid crashes.

---

## Testing & DSP Guidelines

### 1. FFF Mocking Isolation
- Separate Fake Function Framework (FFF) mocks and fake callback definitions from the core test suite file (e.g. put them in a dedicated mocking file or header) rather than defining them inline within test cases.

### 2. Observer & Controller Bandwidth Tuning (ADRC / PID)
- Always tune Observer (LESO) gains mathematically using observer bandwidth $\omega_o$:
  $$L_1 = 2\omega_o, \quad L_2 = \omega_o^2$$
- Prevent integral windup in PID by using dynamic anti-windup clamping limits.

---

## Advanced Architectural Patterns (from Vendor Best Practices)

### 1. Resource-Constrained Concurrent Scheduling
- **Minimize Thread Spawning**: Avoid dedicating threads to low-frequency background tasks (e.g., telemetry, UI updates, battery monitoring) to save SRAM. Use the system work queue (`k_work_submit`) or dedicated low-priority work queues.
- **Event-Driven Communication (ZBUS / uORB)**: Keep modules loosely coupled. Declare event channels using ZBUS/uORB patterns to publish and subscribe to hardware states (e.g., sensor readings, connection changes) asynchronously rather than calling direct APIs across modules.
- **ZBUS Latency & Context Separation**:
  - *ZBUS Synchronous Listeners*: Run in the publisher's thread context (zero-copy direct callback), avoiding message queue latency and scheduling overhead. Use for low-overhead, sequential events.
  - *ZBUS Asynchronous Subscribers*: Use message queues (`k_msgq`) to decouple execution contexts for non-critical paths, preventing blocking behavior in the publisher thread.
  - *Work Queue Context Switching*: `k_work_submit` only incurs minor queue chaining overhead in the ISR, but triggers a `PendSV` thread swap. Use it for medium-latency tasks (e.g., UI, telemetry, Flash storage) rather than raw high-frequency ISR.

### 2. Safe & Deterministic Startup Sequence
- **Asynchronous HW Self-Checks**: To prevent hardware watchdog (WDT) triggers during complex boot sequences, do not run blocking self-checks directly in `main()`. Delegate initialization sequences (e.g., loading settings, initializing file systems) to a work queue.
- **Cooperative Delayed Start for Sensitive Peripherals**: For high-power or high-precision circuits (e.g., ADCs, charge controllers), run the control loops in a cooperative thread (priority < 0) with a startup delay (e.g., 2 seconds) to allow analog power rails and hardware bias voltages to fully stabilize first.

### 3. Hard Real-Time DSP and Control Loops
- **Bypassing OS Context Switch**: For high-frequency loops (FOC motor control, high-frequency buck converters), write calculations directly in the ISR via raw vector hookup (`IRQ_CONNECT`), avoiding thread context switches entirely.
  - *Cortex-M Context Overhead*: Cortex-M hardware stacking (saving `r0-r3, r12, lr, pc, xpsr` in ~12 cycles) is extremely fast. However, software stacking of remaining registers (`r4-r11` during PendSV scheduling) adds 1.5 - 3 microseconds of jitter. Bypassing the scheduler keeps ISR response under 100ns.
- **DSP Mathematical Optimizations**:
  - *Single-Cycle FPU*: Utilize single-cycle FPU instructions (e.g. `VMLA.F32`) and CMSIS-DSP library functions.
  - *Lookup Tables (LUT)*: Use lookup tables for trigonometric calculations (e.g., `sin`/`cos` in Park/Clark transforms) to avoid long execution times.
  - *Eliminate Divisions*: Replace division operations with multiplications of pre-calculated inverses (`1.0f / value`) to save CPU cycles.
  - *Dynamic Memory Ban*: Never use dynamic memory allocation (`malloc`, `free`, or `new`) inside control loops.
- **Cycle-by-Cycle Hardware Safeguards**: Implement high-priority fault stops directly inside the ISR (e.g., stopping PWM/MOSFET output immediately on overcurrent or undervoltage) to protect physical circuits.

### 4. Lockless State & Configuration Registry
- **Lockless Read-Write with Packed Bitfields**: Pack boolean flags into aligned byte/word boundaries (e.g. `struct pwr_flags` packed in `uint16_t` / `uint8_t`). On Cortex-M, simple bitwise operations on byte-aligned bitfields are atomic or single-instruction, removing the need for heavy OS mutexes.
- **Dynamic vs Static Attributes**: Establish a central code-generated attribute table/dictionary for all configuration variables. Initialize attributes on boot from NV storage once, and use fast read-write get/set wrappers instead of scattering `extern` variables throughout the project.

### 5. Post-Crash Diagnostics & Integrity Checks
- **Retention Registers (Warm Reboot Checkpoints)**: Leverage non-volatile Retention RAM (or backup registers on the SoC) to store checkpoint state codes and crash/fault dumps. On boot, read these before initializing other subsystems to diagnose previous watchdog reset reasons without losing transient state.
- **Background Memory Integrity Scans**: Spin up a very low-priority thread to continuously run background CRC/checksum sweeps over active Flash regions to identify memory degradation or bit flips caused by noise in industrial environments.

---

## Next Steps: tinyML & AI Implementation Roadmap

Practice tinyML on the Vango M33 CPU by implementing **NILM (Non-Intrusive Load Monitoring)** for [ref_nilm_meter](file:///workspaces/applications/apps/ref_nilm_meter) and **Predictive Maintenance** for [ref_motor_pred_maint](file:///workspaces/applications/apps/ref_motor_pred_maint). 

### Borrowed Design Patterns (from sdk-edge-ai)

1. **Streaming Ping-Pong Buffers**: 
   Orchestrate the producer-consumer data flow between the Cortex-M0 core (handling high-frequency ADC sampling) and the Cortex-M33 core (running tinyML inference) using standard Zephyr synchronization primitives (`struct k_sem` and `struct k_timer`). Use lockless shared memory in `WAVEFORM_SHM` to pass buffers.
2. **Precomputed Look-Up Tables (LUT)**: 
   Avoid expensive online operations (like transcendental functions, windowing, or quantization scaling) on the M33 CPU. Precompute these tables at boot time (e.g. `prefill_luts`) to keep online pre-processing at $O(1)$ complexity.
3. **Preemptive Low-Priority Inference Thread**: 
   Since model inference is CPU-heavy, run it inside a low-priority preemptible thread (`K_THREAD_DEFINE` with low priority) or system workqueue. High-priority communication loops (such as Modbus RTU client and PPP TCP/IP network tasks) must never be blocked by tinyML computations.
4. **Static Arena Memory Allocation**: 
   Avoid memory fragmentation on resource-constrained MCUs by pre-allocating a static Tensor Arena (e.g., `uint8_t tensor_arena[10240]`) inside `tinyml_service.c` instead of using dynamic `malloc`.

---

## Compilation & Syncing (workflow.sh)
- **RAM-Disk Syncing**: Always execute compilation through `./workflow.sh build <app> <profile>`. Raw `west build` ignores the RAM-disk sync (`/root/fast_space`) and custom `ZEPHYR_MODULES` paths, resulting in outdated or failed builds.
- **Bypass Sandbox Requirement**: The compilation toolchain and RAM-disk live outside the workspace container boundary. You must run build commands with sandbox bypass enabled.

