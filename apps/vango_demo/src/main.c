/*
 * Unified Application Orchestrator
 * (SoC-agnostic, capability-driven)
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

#if defined(CONFIG_SOC_V32F20X_CPUAPP)
#include <zephyr/sys/reboot.h>
extern void boot_cpu1(void);
#endif

extern int ipc_service_init(void);
#ifdef CONFIG_APP_FEATURE_LOW_POWER
extern int power_monitor_init(void);
extern int low_power_init(void);
#endif
#ifdef CONFIG_APP_FEATURE_OTA
extern int ota_service_init(void);
#endif
extern int watchdog_init(void);
extern void watchdog_feed(void);

#if defined(CONFIG_SOC_V32F20X_CPUMETER)
#include <protocol.h>
extern int ipc_send_metering(struct metering_payload *payload);
extern int ipc_send_waveform_ptr(struct waveform_ptr_payload *payload);
#endif

int main(void)
{
        LOG_INF("============================================");
        LOG_INF("  Vango Target-Centric App Orchestrator");
        LOG_INF("  Build: %s %s", __DATE__, __TIME__);
        LOG_INF("============================================");

#if defined(CONFIG_SOC_V32F20X_CPUAPP)
    LOG_INF("Core: CPUAPP (Cortex-M33 Primary Core)");
    LOG_INF("Booting CPU1 (cpumeter) from separate sysbuild image...");
    boot_cpu1();
#elif defined(CONFIG_SOC_V32F20X_CPUMETER)
    LOG_INF("Core: CPUMETER (Cortex-M0 Secondary Core)");
#endif

    /* Initialize Services (Note: FlashDB is now handled via SYS_INIT) */
    watchdog_init();
    ipc_service_init();
#ifdef CONFIG_APP_FEATURE_LOW_POWER
    power_monitor_init();
    low_power_init();
#endif
#ifdef CONFIG_APP_FEATURE_OTA
    ota_service_init();
#endif

#if defined(CONFIG_SOC_V32F20X_CPUMETER)
    struct metering_payload mp = {0};
    struct waveform_ptr_payload wp = {
        .shm_address = WAVEFORM_SHM_BASE,
        .sample_cnt = 512,
        .channel_mask = 0x07
    };
#endif

        while (1) {
                watchdog_feed();
        #if defined(CONFIG_SOC_V32F20X_CPUMETER)
                /* 1. Periodic Standard Metering Data */
                mp.active_energy += 10;
                mp.reactive_energy += 5;
                mp.timestamp = k_uptime_get_32();
                ipc_send_metering(&mp);

                /* 2. Zero-copy Waveform Event */
                ipc_send_waveform_ptr(&wp);
        #endif
                k_msleep(10000);
        }
        return 0;
}
