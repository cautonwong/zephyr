/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>

LOG_MODULE_REGISTER(pwr_mon, LOG_LEVEL_INF);

typedef void (*ana_cmp_callback_t)(const struct device *dev);
extern int ana_cmp_v32f20x_set_callback(const struct device *dev, ana_cmp_callback_t cb);

static void power_fail_handler(const struct device *dev)
{
    LOG_ERR("!!!! POWER FAILURE DETECTED !!!!");
    
    /* 3. Transition to STANDBY / Deep Sleep */
    pm_state_force(0, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
}

int power_monitor_init(void)
{
    const struct device *cmp = DEVICE_DT_GET_ANY(vango_v32f20x_ana_cmp);
    
    if (cmp == NULL || !device_is_ready(cmp)) {
        LOG_WRN("Comparator not available for power monitoring on this core");
        return -ENODEV;
    }

    ana_cmp_v32f20x_set_callback(cmp, power_fail_handler);
    LOG_INF("Power monitoring active (Analog Comparator)");
    return 0;
}
