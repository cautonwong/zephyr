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

/* Work item for executing blocking operations (FlashDB write) outside ISR */
static struct k_work power_fail_work;

/* 
 * Zephyr Best Practice: The ISR should be extremely short.
 * We must not call Flash APIs or sleep within an ISR.
 * We submit a high-priority work item instead.
 */
static void power_fail_isr_callback(const struct device *dev)
{
    /* 
     * Optional: Extremely fast, non-blocking hardware actions can go here.
     * e.g., Set a GPIO to trigger an external hardware latch.
     */
    
    /* Submit work to the system workqueue to handle the power-down sequence */
    k_work_submit(&power_fail_work);
}

/*
 * Emergency Handler: Executed in Thread Context
 */
static void power_fail_worker(struct k_work *item)
{
    LOG_ERR("!!!! EMERGENCY: POWER FAILURE DETECTED !!!!");

    /* 1. Prevent context switching to ensure emergency tasks complete */
    k_sched_lock();

    /* 2. Emergency Data Save (FlashDB) 
     *    Call your specific FlashDB KV/TS save APIs here.
     *    e.g., fdb_kv_set("power_lost", "1");
     */
    LOG_WRN("-> Flushing critical data to FlashDB...");

    /* 3. Disable power-hungry peripherals (e.g., Modem, Modbus) */
    LOG_WRN("-> Suspending non-critical devices...");
#if defined(CONFIG_MODEM)
    const struct device *modem = DEVICE_DT_GET_ANY(quectel_bg9x);
    if (device_is_ready(modem)) {
        pm_device_action_run(modem, PM_DEVICE_ACTION_SUSPEND);
    }
#endif

    /* 4. Downclock the CPU to save remaining capacitor energy */
    LOG_WRN("-> Scaling down system clock...");
    /* e.g., CLK_SetSysClock(CLK_SOURCE_INTERNAL_8M); */

    /* 5. Force the system into Deep Sleep (STANDBY / SOFT_OFF) */
    LOG_WRN("-> Entering STANDBY mode...");
    k_sched_unlock(); /* Must unlock before forcing PM state */
    
    pm_state_force(0, &(struct pm_state_info){PM_STATE_SOFT_OFF, 0, 0});
    
    /* Should never reach here if STANDBY is successful */
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
    const struct device *cmp = DEVICE_DT_GET_ANY(vango_v32f20x_ana_cmp);
    if (cmp == NULL || !device_is_ready(cmp)) {
        LOG_WRN("Comparator not available for power monitoring on this core");
        return -ENODEV;
    }

    ana_cmp_v32f20x_set_callback(cmp, power_fail_isr_callback);
    LOG_INF("Power monitoring active (Analog Comparator)");
    
    return 0;
}

