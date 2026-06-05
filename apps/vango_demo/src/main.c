/*
 * Unified Application Orchestrator
 * (SoC-agnostic, capability-driven)
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

#if defined(CONFIG_SOC_V32F20X_CPUAPP)
static void boot_cpu1(void)
{
    LOG_INF("Releasing CM0 (Metering) core reset...");
    /* V32F20X SYSCFGLP CM0_CTRL: bit 0 is Reset (1=Reset, 0=Run) */
    *(volatile uint32_t *)0x40102050 = 0;
}
#endif

#ifdef CONFIG_COREDUMP
void crash_test(void)
{
    LOG_ERR("!!! Intentional Crash for Coredump Demo !!!");
    volatile int *ptr = NULL;
    *ptr = 0xDEADBEEF;
}
#endif

extern int ipc_service_init(void);
#ifdef CONFIG_APP_FEATURE_LOW_POWER
extern int power_monitor_init(void);
extern int low_power_init(void);
#endif
#ifdef CONFIG_APP_FEATURE_OTA
extern int ota_service_init(void);
#endif
#if defined(CONFIG_SOC_V32F20X_CPUMETER)
#include <protocol.h>
extern int ipc_send_metering(struct metering_data *data);
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
    ipc_service_init();
#ifdef CONFIG_APP_FEATURE_LOW_POWER
    power_monitor_init();
    low_power_init();
#endif
#ifdef CONFIG_APP_FEATURE_OTA
    ota_service_init();
#endif

#if defined(CONFIG_SOC_V32F20X_CPUMETER)
    struct metering_data md = {0};
#endif

        while (1) {
        #if defined(CONFIG_SOC_V32F20X_CPUMETER)
                /* Simulate production metering data pulse */
                md.active_energy += 10;
                md.reactive_energy += 5;
                md.timestamp = k_uptime_get_32();
                ipc_send_metering(&md);
        #endif
                k_msleep(10000);
        }
        return 0;
}
