# PRD: Advanced Power Management (Device Runtime PM)

## Problem Statement
The current system configuration does not prioritize power efficiency. For collector nodes (V85XXP) or battery-operated devices, the high base current prevents long-term deployment. External peripherals like UART and Flash remain powered even when idle, leading to unnecessary energy consumption.

## Solution
Implement a comprehensive power management strategy using Zephyr's PM subsystem and Device Runtime PM. This includes defining low-power states (Sleep, Deep Sleep) and adding power management hooks to custom drivers (UART, Flash) so they enter ultra-low power modes automatically when not in use.

## User Stories
1. As a battery device engineer, I want the system to consume < 10uA in sleep mode so that the battery lasts for years.
2. As a firmware developer, I want my drivers to automatically power down when idle without me having to call manual `power_off` APIs.
3. As a system monitor, I want to see the power state of each peripheral via a shell command so that I can debug power leaks.

## Implementation Decisions
- **Power Subsystem**: Enable `CONFIG_PM` and `CONFIG_PM_DEVICE`.
- **Runtime PM**: Configure `CONFIG_PM_DEVICE_RUNTIME=y` to allow devices to be suspended when their usage count drops to zero.
- **Driver Hooks**: Implement the `pm_device_action_cb_t` in the V32/V85 UART and Flash drivers.
  - *UART*: Suspend hardware clocks and disable transceiver when idle for 1s.
  - *Flash*: Enter "Deep Power Down" mode of the Vango flash controller.
- **System Idle**: Configure the Zephyr idle thread to enter the lowest possible CPU sleep state (`STOP` or `STANDBY`) when no threads are active.

## Testing Decisions
- **Current Measurement**: Use a high-precision ammeter (like Power Profiler Kit II) to verify the current drops to the micro-amp level when the system is idle.
- **Wake-up Latency**: Verify that the system can wake up from deep sleep within 100ms upon a GPIO or UART interrupt without losing data.
- **PM Shell Verification**: Use `device list` and `pm get` shell commands to confirm peripherals are indeed in the `SUSPENDED` state.

## Out of Scope
- Dynamic voltage scaling (DVS).
- Power management for high-power external modules like 4G (will be handled by modem-specific sleep AT commands).
