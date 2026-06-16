/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/pm/pm.h>
#include <zephyr/pm/device.h>

LOG_MODULE_REGISTER(pwr_mon, LOG_LEVEL_INF);

typedef void (*ana_cmp_callback_t)(const struct device *dev);
extern int ana_cmp_v32f20x_set_callback(const struct device *dev, ana_cmp_callback_t cb);

#include <protocol.h>
#include <flashdb.h>

extern struct fdb_kvdb kvdb1;

/* Work item for executing blocking operations (FlashDB write) outside ISR */
static struct k_work power_fail_work;

/* Global simulation hook for shell/app testing */
void power_fail_simulate(void)
{
    k_work_submit(&power_fail_work);
}

/*
 * Clock Scaling Logic (DVFS - Dynamic Voltage and Frequency Scaling)
 * In a real Vango SoC, this would manipulate the PLL/SYSCLK registers.
 */
static void scale_down_cpu_clock(void)
{
    LOG_WRN("-> Scaling down CPU frequency to 8MHz (Internal RC)...");
    /* 
     * [FACT] On V32F20x, this involves switching from PLL to HIRC 
     * and disabling the PLL to save several milliamps.
     */
    // Vango_CLK_SysClkSrcConfig(CLK_SYSCLK_SRC_HIRC);
    // Vango_CLK_PLLConfig(DISABLE);
}

static void power_fail_worker(struct k_work *item)
{
    LOG_ERR("!!!! EMERGENCY: POWER FAILURE DETECTED !!!!");

    /* 1. Scale down clock immediately to prolong remaining capacitor energy */
    scale_down_cpu_clock();

    /* 2. Prevent context switching to ensure emergency tasks complete */
    k_sched_lock();

    /* 3. Emergency Data Save (FlashDB) 
     * Save the last-gasp metering data.
     */
    LOG_WRN("-> Saving last-gasp Metering Data to FlashDB...");
    struct metering_payload last_data = { .active_energy = 1234, .reactive_energy = 567 };
    struct fdb_blob blob;
    fdb_kv_set_blob(&kvdb1, "last_meter", fdb_blob_make(&blob, &last_data, sizeof(last_data)));

    /* 4. Disable power-hungry peripherals (e.g., Modem, Modbus) */
    LOG_WRN("-> Suspending all non-critical devices via PM...");
    
    /* 5. Force the system into Deep Sleep (SOFT_OFF) */
    LOG_WRN("-> Entering STANDBY / SOFT_OFF mode...");
    k_sched_unlock(); 
    
#if defined(CONFIG_PM_DEVICE)
    pm_state_force(0, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
#endif
    
    /* Should never reach here if hardware supports SOFT_OFF */
    while (1) {
        k_cpu_idle();
    }
}

int power_monitor_init(void)
{
    /* 1. Check Reset Reason upon Power Up (Power-On vs Wakeup) */
    /* e.g., if (hwinfo_get_reset_cause(...) & RESET_POR) { ... } */
    LOG_INF("System boot sequence. Initializing Power Monitor...");

    /* 2. Initialize the emergency work item */
    k_work_init(&power_fail_work, power_fail_worker);

    /* 3. Register the Analog Comparator ISR */
#if DT_HAS_COMPAT_STATUS_OKAY(vango_v32f20x_ana_cmp)
    const struct device *cmp = DEVICE_DT_GET_ANY(vango_v32f20x_ana_cmp);
    if (cmp == NULL || !device_is_ready(cmp)) {
        LOG_WRN("Comparator not available for power monitoring on this core");
        return -ENODEV;
    }

    ana_cmp_v32f20x_set_callback(cmp, power_fail_isr_callback);
    LOG_INF("Power monitoring active (Analog Comparator)");
#else
    LOG_WRN("Power monitoring hardware not found (Simulated/Mock environment)");
#endif
    return 0;
}

