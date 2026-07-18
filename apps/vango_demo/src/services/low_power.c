/*
 * Event-Driven Power Management Service
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#if defined(CONFIG_APP_FEATURE_LOW_POWER)

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(low_power, LOG_LEVEL_INF);

/* Standard device list for runtime PM */
static const char *monitored_devices[] = {
    "uart0",
    "uart14",
    "modbus0"
};

#define DEVICE_COUNT (sizeof(monitored_devices) / sizeof(monitored_devices[0]))

/* Get simulated Axon device reference directly from Devicetree */
#if DT_NODE_HAS_STATUS_OKAY(DT_NODELABEL(sim_axon))
static const struct device *const sim_axon_dev = DEVICE_DT_GET(DT_NODELABEL(sim_axon));
#else
static const struct device *const sim_axon_dev = NULL;
#endif

/* Asynchronous work item for simulated tinyML inference completion */
static struct k_work infer_work;
static const struct shell *shell_ctx;

int axon_power_request(void)
{
    if (sim_axon_dev == NULL || !device_is_ready(sim_axon_dev)) {
        LOG_ERR("Simulated AXON device not ready or disabled");
        return -ENODEV;
    }
    LOG_INF("axon_power_request() -> Voting for Axon power up.");
    return pm_device_runtime_get(sim_axon_dev);
}

int axon_power_release(void)
{
    if (sim_axon_dev == NULL || !device_is_ready(sim_axon_dev)) {
        return -ENODEV;
    }
    LOG_INF("axon_power_release() -> Releasing Axon power vote.");
    return pm_device_runtime_put(sim_axon_dev);
}

static void infer_work_handler(struct k_work *work)
{
    ARG_UNUSED(work);

    LOG_INF("[tinyML] Running high-precision NILM inference algorithm...");
    /* Simulate 500ms model computation time on CPU */
    k_msleep(500);

    LOG_INF("[tinyML] NILM CLASSIFY SUCCESS: Washing Machine Detected (Confidence: 94%%, Active Power: 1850W).");

    /* Completed. Release power vote in work queue context */
    axon_power_release();

    if (shell_ctx) {
        shell_print(shell_ctx, "Inference completed successfully. Result logged.");
    }
}

/**
 * @brief Initialize Device Runtime PM for all target devices.
 */
int low_power_init(void)
{
    LOG_INF("Initializing Event-Driven PM (Runtime PM Enabled)");

    for (size_t i = 0; i < DEVICE_COUNT; i++) {
        const struct device *dev = device_get_binding(monitored_devices[i]);
        if (dev) {
            int ret = pm_device_runtime_enable(dev);
            if (ret == 0) {
                LOG_INF("  [OK] %s: Autonomous PM active", monitored_devices[i]);
            } else {
                LOG_WRN("  [FAIL] %s: Runtime PM not supported or already enabled", monitored_devices[i]);
            }
        }
    }

    /* Enable runtime PM for simulated Axon device */
    if (sim_axon_dev && device_is_ready(sim_axon_dev)) {
        int ret = pm_device_runtime_enable(sim_axon_dev);
        if (ret == 0) {
            LOG_INF("  [OK] sim_axon: Autonomous PM active");
        } else {
            LOG_WRN("  [FAIL] sim_axon: Runtime PM enable failed");
        }
    }

    /* Initialize inference work item */
    k_work_init(&infer_work, infer_work_handler);

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

    /* Print simulated Axon state */
    if (sim_axon_dev) {
        enum pm_device_state state;
        (void)pm_device_state_get(sim_axon_dev, &state);
        bool is_enabled = pm_device_runtime_is_enabled(sim_axon_dev);
        shell_print(sh, "%-15s : %-10s | RuntimePM: %s", 
                    "sim_axon", 
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

static int cmd_pm_infer_sim(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Starting simulated tinyML NILM inference task...");
    shell_ctx = sh;

    /* 1. Request power vote before model execution */
    int ret = axon_power_request();
    if (ret < 0 && ret != -EALREADY) {
        shell_error(sh, "Failed to vote for Axon power up (err %d)", ret);
        return ret;
    }

    /* 2. Submit async work item representing model execution in work queue context */
    k_work_submit(&infer_work);
    
    return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(sub_pm,
    SHELL_CMD_ARG(status, NULL, "Show autonomous PM status.", cmd_pm_status, 1, 0),
    SHELL_CMD_ARG(trigger_fail, NULL, "Simulate an emergency power loss event.", cmd_pm_trigger_fail, 1, 0),
    SHELL_CMD_ARG(infer_sim, NULL, "Trigger a simulated tinyML inference event with PM Runtime voting.", cmd_pm_infer_sim, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(pm, &sub_pm, "Advanced PM commands", NULL);

#endif /* CONFIG_APP_FEATURE_LOW_POWER */
