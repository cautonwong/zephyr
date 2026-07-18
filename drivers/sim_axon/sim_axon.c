/*
 * Simulated Axon Accelerator Device Driver
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/pm/device.h>
#include <zephyr/pm/device_runtime.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(sim_axon, LOG_LEVEL_INF);

#define DT_DRV_COMPAT vango_sim_axon

#if DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0

struct sim_axon_config {
    const struct pinctrl_dev_config *pcfg;
    uint32_t reg_base;
};

struct sim_axon_data {
    bool is_powered;
};

static int sim_axon_pm_action(const struct device *dev, enum pm_device_action action)
{
    const struct sim_axon_config *config = dev->config;
    struct sim_axon_data *data = dev->data;
    int ret;

    switch (action) {
    case PM_DEVICE_ACTION_RESUME:
        ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_DEFAULT);
        if (ret < 0) {
            LOG_ERR("Failed to set default pinctrl state (err %d)", ret);
            return ret;
        }
        data->is_powered = true;
        LOG_INF("-> [PM] AXON Accelerator RESUMED. Pinctrl state: DEFAULT (Pull-up).");
        break;

    case PM_DEVICE_ACTION_SUSPEND:
        ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_SLEEP);
        if (ret < 0) {
            LOG_ERR("Failed to set sleep pinctrl state (err %d)", ret);
            return ret;
        }
        data->is_powered = false;
        LOG_INF("-> [PM] AXON Accelerator SUSPENDED. Pinctrl state: SLEEP (Pull-down).");
        break;

    default:
        return -ENOTSUP;
    }

    return 0;
}

PM_DEVICE_DT_INST_DEFINE(0, sim_axon_pm_action);

static int sim_axon_init(const struct device *dev)
{
    const struct sim_axon_config *config = dev->config;
    int ret;

    ret = pinctrl_apply_state(config->pcfg, PINCTRL_STATE_SLEEP);
    if (ret < 0) {
        LOG_ERR("Failed to set initial sleep state (err %d)", ret);
        return ret;
    }

    LOG_INF("Simulated AXON initialized successfully. Status: Idle / Sleep.");
    return 0;
}

PINCTRL_DT_INST_DEFINE(0);

static const struct sim_axon_config sim_axon_config_0 = {
    .pcfg = PINCTRL_DT_INST_DEV_CONFIG_GET(0),
    .reg_base = DT_INST_REG_ADDR(0),
};

static struct sim_axon_data sim_axon_data_0 = {
    .is_powered = false,
};

DEVICE_DT_INST_DEFINE(0,
    sim_axon_init,
    PM_DEVICE_DT_INST_GET(0),
    &sim_axon_data_0,
    &sim_axon_config_0,
    POST_KERNEL,
    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
    NULL
);


#endif /* DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) > 0 */
