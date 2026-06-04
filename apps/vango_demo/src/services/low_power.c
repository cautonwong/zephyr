/*
 * Event-Driven Power Management Service
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(low_power, LOG_LEVEL_INF);

/* 
 * DEVICES LIST
 * These devices will use standard Zephyr Runtime PM.
 * Autonomous logic: No periodic scanning required.
 */
static const char *monitored_devices[] = {
    "uart0",
    "uart14",
    "modbus0"
};

#define DEVICE_COUNT (sizeof(monitored_devices) / sizeof(monitored_devices[0]))

/**
 * @brief Initialize Device Runtime PM for all target devices.
 * This allows the drivers to manage their own power states 
 * (e.g., sleep when idle, wake on activity) without a supervisor thread.
 */
int low_power_init(void)
{
    LOG_INF("Initializing Event-Driven PM (Runtime PM Enabled)");

    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        const struct device *dev = device_get_binding(monitored_devices[i]);
        if (dev) {
            /* Enable Zephyr's internal usage-counting PM */
            int ret = pm_device_runtime_enable(dev);
            if (ret == 0) {
                LOG_INF("  [OK] %s: Autonomous PM active", monitored_devices[i]);
            } else {
                LOG_WRN("  [FAIL] %s: Runtime PM not supported or already enabled", monitored_devices[i]);
            }
        }
    }

    return 0;
}

/* --- Emergency Logic Hook --- */
extern void power_fail_simulate(void);

/* --- Shell Commands --- */

static int cmd_pm_status(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Device Runtime PM Status:");
    shell_print(sh, "-------------------------");
    
    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        const struct device *dev = device_get_binding(monitored_devices[i]);
        if (!dev) continue;

        enum pm_device_state state;
        (void)pm_device_state_get(dev, &state);
        
        bool is_enabled = pm_device_runtime_is_enabled(dev);
        
        shell_print(sh, "%-15s : %-10s | RuntimePM: %s", 
                    monitored_devices[i], 
                    (state == PM_DEVICE_STATE_ACTIVE) ? "ACTIVE" : "SUSPENDED",
                    is_enabled ? "ON" : "OFF");
    }
    return 0;
}

static int cmd_pm_trigger_fail(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "!!! SIMULATING SYSTEM POWER FAILURE !!!");
    power_fail_simulate();
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_pm,
    SHELL_CMD_ARG(status, NULL, "Show autonomous PM status.", cmd_pm_status, 1, 0),
    SHELL_CMD_ARG(trigger_fail, NULL, "Simulate an emergency power loss event.", cmd_pm_trigger_fail, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(pm, &sub_pm, "Advanced PM commands", NULL);
