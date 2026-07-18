/*
 * Common Middleware Core for Industrial Reference Designs
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef COMMON_CORE_H
#define COMMON_CORE_H

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Shared metrics structure for energy-oriented products (NILM, BMS, V2G) */
struct power_metrics {
    float voltage;
    float current;
    float active_power;
    float reactive_power;
    float frequency;
    uint32_t timestamp;
};

/* Common core initialization helpers */
static inline void common_core_init(const char *app_name)
{
    printk("\n========================================\n");
    printk("Starting Industrial Reference Design: %s\n", app_name);
    printk("Architecture: Cortex-M33 + Cortex-M0 Dual-core\n");
    printk("OS: Zephyr RTOS with TF-M Security Profile\n");
    printk("========================================\n\n");
}

#endif /* COMMON_CORE_H */
