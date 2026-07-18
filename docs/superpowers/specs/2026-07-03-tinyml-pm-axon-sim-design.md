# Design Spec: Simulated Axon Accelerator & PM Runtime Integration

This document specifies the architecture and implementation design for integrating a simulated Axon hardware accelerator driver and dynamic power management runtime (`onoff_manager`) into the Vango Technologies Zephyr RTOS applications project.

---

## 1. DeviceTree Overlay and Binding Configuration

We define a virtual hardware node for the simulated Axon device to intercept power state changes.

### 1.1 DeviceTree Binding
Create a new Devicetree binding file: [vango,sim-axon.yaml](file:///workspaces/applications/dts/bindings/vango,sim-axon.yaml)

```yaml
description: Vango Technologies Simulated Axon Accelerator for tinyML PM Verification
compatible: "vango,sim-axon"
include: [base.yaml, pinctrl-device.yaml]

properties:
  reg:
    type: array
    required: true
  status:
    type: string
    required: true
```

### 1.2 DeviceTree Overlay
Add the `sim_axon` node and corresponding `pinctrl` states to [app.overlay](file:///workspaces/applications/apps/vango_demo/targets/v32_cpuapp_gateway_ns/app.overlay) and [v32_cpuapp_gateway/app.overlay](file:///workspaces/applications/apps/vango_demo/targets/v32_cpuapp_gateway/app.overlay):

```dts
&pinctrl {
    axon_default: axon_default {
        group1 {
            pinmux = <VANGO_PINMUX(VANGO_PORT_A, 4, 0)>; /* Port A Pin 4 */
            bias-pull-up;
        };
    };
    axon_sleep: axon_sleep {
        group1 {
            pinmux = <VANGO_PINMUX(VANGO_PORT_A, 4, 0)>;
            bias-pull-down;
        };
    };
};

/ {
    sim_axon: sim_axon@4002c000 {
        compatible = "vango,sim-axon";
        reg = <0x4002c000 0x1000>;
        pinctrl-0 = <&axon_default>;
        pinctrl-1 = <&axon_sleep>;
        pinctrl-names = "default", "sleep";
        status = "okay";
    };
};
```

---

## 2. Simulated Axon Driver (`sim_axon.c`)

The driver registers itself under the Zephyr device model and handles power transitions (`PM_DEVICE_ACTION_RESUME` and `PM_DEVICE_ACTION_SUSPEND`), updating the pinctrl state of simulated GPIO pins.

- File Location: [sim_axon.c](file:///workspaces/applications/drivers/sim_axon/sim_axon.c)
- Implementation details:
  - Initializes the device in `POST_KERNEL` phase.
  - Implements the PM action handler callback to switch pinctrl to `PINCTRL_STATE_DEFAULT` when active, and `PINCTRL_STATE_SLEEP` when suspended.

---

## 3. Power Management Service Integration (`low_power.c` / `onoff_manager`)

We integrate the `onoff_manager` in [low_power.c](file:///workspaces/applications/apps/vango_demo/src/services/low_power.c) to handle application-level power voting.

### 3.1 Initializing `onoff_manager`
- Define a static `struct onoff_manager axon_pm_mgr`.
- Initialize it using `onoff_manager_init(&axon_pm_mgr, ...)` or a custom power transition helper.
- Enable `pm_device_runtime_enable()` on the `"sim_axon"` device so that usage-based voting propagates to the driver PM action handler automatically.

### 3.2 Power Request and Release Flow
- **Request**: Prior to invoking simulated tinyML inference, the application calls `axon_power_request()`, which calls `onoff_request()` or `pm_device_runtime_get(sim_axon_dev)`.
- **Release**: Upon completion of the workload, the system workqueue calls `axon_power_release()`, invoking `onoff_release()` or `pm_device_runtime_put(sim_axon_dev)`.

---

## 4. Verification Plan

1. **Compilation Check**: Run `./workflow.sh build vango_demo v32_cpuapp_gateway` to verify that the devicetree bindings, overlays, driver code, and service integrations compile without warnings.
2. **Log Verification**: Run the simulation target using Renode and verify in the console logs that:
   - `Simulated AXON initialized successfully. Status: Idle / Sleep.` prints during boot.
   - Running the simulated tinyML inference command triggers:
     - `-> [PM] AXON Accelerator RESUMED. Pinctrl state: DEFAULT (Pull-up).`
     - `-> [PM] AXON Accelerator SUSPENDED. Pinctrl state: SLEEP (Pull-down).`
