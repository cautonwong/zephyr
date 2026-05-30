/*
 * Advanced Power Management (Device Runtime PM) Service
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(low_power, LOG_LEVEL_INF);

#define LP_STACK_SIZE 1024
#define LP_PRIORITY   6

static struct k_thread lp_thread_data;
static K_KERNEL_STACK_DEFINE(lp_stack, LP_STACK_SIZE);

/* We will monitor these devices for runtime PM simulation */
static const char *monitored_devices[] = {
    "uart0",
    "uart14",
    "modbus0"
};

#define DEVICE_COUNT (sizeof(monitored_devices) / sizeof(monitored_devices[0]))

/* Track activity timestamps */
static uint64_t last_activity_time = 0;

void low_power_report_activity(void)
{
    last_activity_time = k_uptime_get();
}

static void low_power_monitor_task(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    LOG_INF("Advanced Power Management Service Initialized");
    last_activity_time = k_uptime_get();

    /* Ensure device runtime PM is enabled for our targets */
    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        const struct device *dev = device_get_binding(monitored_devices[i]);
        if (dev) {
            pm_device_runtime_enable(dev);
            LOG_INF("Runtime PM enabled for device: %s", monitored_devices[i]);
        }
    }

    while (1) {
        k_sleep(K_MSEC(1000));
        uint64_t now = k_uptime_get();
        
        /* Saturation check: suspend monitored peripherals if idle for > 5 seconds */
        if (now - last_activity_time > 5000) {
            for (size_t i = 0; i < DEVICE_COUNT; i++) {
                const struct device *dev = device_get_binding(monitored_devices[i]);
                if (dev) {
                    enum pm_device_state state;
                    (void)pm_device_state_get(dev, &state);
                    if (state == PM_DEVICE_STATE_ACTIVE) {
                        LOG_INF("System idle: Automatically Suspending device: %s", monitored_devices[i]);
                        int ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
                        if (ret && ret != -EALREADY) {
                            LOG_WRN("Failed to suspend %s: %d", monitored_devices[i], ret);
                        }
                    }
                }
            }
        }
    }
}

int low_power_init(void)
{
    k_thread_create(&lp_thread_data, lp_stack, K_KERNEL_STACK_SIZEOF(lp_stack),
                    low_power_monitor_task, NULL, NULL, NULL,
                    LP_PRIORITY, 0, K_NO_WAIT);
    k_thread_name_set(&lp_thread_data, "low_power_monitor");
    return 0;
}

/* --- Shell Commands --- */

static int cmd_power_list(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Monitored Devices Power States:");
    shell_print(sh, "---------------------------------");
    
    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        const struct device *dev = device_get_binding(monitored_devices[i]);
        if (!dev) {
            shell_print(sh, "%-15s : NOT FOUND", monitored_devices[i]);
            continue;
        }

        enum pm_device_state state;
        int ret = pm_device_state_get(dev, &state);
        const char *state_str = "UNKNOWN";

        if (ret == 0) {
            switch (state) {
            case PM_DEVICE_STATE_ACTIVE:
                state_str = "ACTIVE";
                break;
            case PM_DEVICE_STATE_SUSPENDED:
                state_str = "SUSPENDED";
                break;
            case PM_DEVICE_STATE_SUSPENDING:
                state_str = "SUSPENDING";
                break;
            case PM_DEVICE_STATE_OFF:
                state_str = "OFF";
                break;
            }
        }
        shell_print(sh, "%-15s : %s", monitored_devices[i], state_str);
    }
    return 0;
}

static int cmd_power_suspend(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: power suspend <device_name>");
        return -EINVAL;
    }

    const struct device *dev = device_get_binding(argv[1]);
    if (!dev) {
        shell_error(sh, "Device '%s' not found", argv[1]);
        return -ENODEV;
    }

    shell_print(sh, "Transitioning '%s' to SUSPENDED...", argv[1]);
    int ret = pm_device_action_run(dev, PM_DEVICE_ACTION_SUSPEND);
    if (ret == 0) {
        shell_print(sh, "Device '%s' suspended successfully.", argv[1]);
    } else {
        shell_error(sh, "Failed to suspend '%s' (Error: %d)", argv[1], ret);
    }
    return ret;
}

static int cmd_power_resume(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: power resume <device_name>");
        return -EINVAL;
    }

    const struct device *dev = device_get_binding(argv[1]);
    if (!dev) {
        shell_error(sh, "Device '%s' not found", argv[1]);
        return -ENODEV;
    }

    shell_print(sh, "Transitioning '%s' to ACTIVE...", argv[1]);
    int ret = pm_device_action_run(dev, PM_DEVICE_ACTION_RESUME);
    if (ret == 0) {
        shell_print(sh, "Device '%s' resumed successfully.", argv[1]);
        /* Report activity to delay auto-sleep */
        low_power_report_activity();
    } else {
        shell_error(sh, "Failed to resume '%s' (Error: %d)", argv[1], ret);
    }
    return ret;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_power,
    SHELL_CMD_ARG(list, NULL, "List all power-monitored devices.", cmd_power_list, 1, 0),
    SHELL_CMD_ARG(suspend, NULL, "Suspend a specific device.", cmd_power_suspend, 2, 0),
    SHELL_CMD_ARG(resume, NULL, "Resume a specific device.", cmd_power_resume, 2, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(power, &sub_power, "Power Management Commands", NULL);
