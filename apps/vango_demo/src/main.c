/*
 * Unified Application Orchestrator
 * (SoC-agnostic, capability-driven)
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

int main(void)
{
	LOG_INF("============================================");
	LOG_INF("  Vango Target-Centric App Orchestrator");
	LOG_INF("  Build: %s %s", __DATE__, __TIME__);
	LOG_INF("============================================");

#if defined(CONFIG_APP_FEATURE_METERING)
    LOG_INF("--> [ENABLED] Metering Capability");
#endif

#if defined(CONFIG_APP_FEATURE_COMMS)
    LOG_INF("--> [ENABLED] Connectivity Capability");
#endif

#if defined(CONFIG_APP_FEATURE_SECURITY)
    LOG_INF("--> [ENABLED] Security Engine Demonstrations");
#endif

#if defined(CONFIG_APP_FEATURE_LOW_POWER)
    LOG_INF("--> [ENABLED] Low Power Monitoring");
#endif

	while (1) {
		k_msleep(10000);
	}

	return 0;
}
