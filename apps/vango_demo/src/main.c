/*
 * Unified Application Orchestrator
 * (SoC-agnostic, capability-driven)
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef CONFIG_FLASHDB
#include <flashdb.h>
extern fdb_time_t fdb_get_time_impl(void);
#endif

LOG_MODULE_REGISTER(app_main, LOG_LEVEL_INF);

#ifdef CONFIG_FLASHDB
struct fdb_kvdb kvdb1;
struct fdb_tsdb tsdb1;
#endif
static struct fdb_default_kv_node default_kv_table[] = {
    {"boot_count", "0", sizeof("0") - 1},
};

static bool ts_list_cb(fdb_tsl_t tsl, void *arg)
{
    struct fdb_blob blob;
    int temp = 0;
    
    fdb_blob_read((fdb_db_t)&tsdb1, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &temp, sizeof(temp))));
    LOG_INF("[TSDB] time: %d, temp: %d", tsl->time, temp);
    return false;
}

#ifdef CONFIG_FLASHDB
void fdb_test(void)
{
    fdb_err_t result;
    struct fdb_blob blob;
    uint32_t boot_count = 0;
    struct fdb_default_kv default_kv;

    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

    /* Initialize KVDB */
    result = fdb_kvdb_init(&kvdb1, "kvdb1", "storage", &default_kv, NULL);
    if (result != FDB_NO_ERR) {
        LOG_ERR("KVDB init failed: %d", result);
        return;
    }

    /* Get boot_count */
    fdb_kv_get_blob(&kvdb1, "boot_count", fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));
    boot_count++;
    LOG_INF("Boot count: %u", boot_count);

    /* Save boot_count */
    fdb_kv_set_blob(&kvdb1, "boot_count", fdb_blob_make(&blob, &boot_count, sizeof(boot_count)));

    /* Initialize TSDB */
    result = fdb_tsdb_init(&tsdb1, "tsdb1", "storage", fdb_get_time_impl, 128, NULL);
    if (result != FDB_NO_ERR) {
        LOG_ERR("TSDB init failed: %d", result);
        return;
    }

    /* Append log to TSDB */
    int current_temp = 25 + (boot_count % 10);
    fdb_tsl_append(&tsdb1, fdb_blob_make(&blob, &current_temp, sizeof(current_temp)));
    LOG_INF("Appended temperature: %d to TSDB", current_temp);

    /* Iterate over TSDB logs */
    LOG_INF("Iterating over TSDB:");
    fdb_tsl_iter(&tsdb1, ts_list_cb, NULL);
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
extern int power_monitor_init(void);
#if defined(CONFIG_SOC_V32F20X_CPU0)
#include <protocol.h>
extern int ipc_send_metering(struct metering_data *data);
#endif

int main(void)
{
        LOG_INF("============================================");
        LOG_INF("  Vango Target-Centric App Orchestrator");
        LOG_INF("  Build: %s %s", __DATE__, __TIME__);
        LOG_INF("============================================");

    /* Initialize IPC Infrastructure */
    ipc_service_init();
    power_monitor_init();

#ifdef CONFIG_FLASHDB
    fdb_test();
#endif

#if defined(CONFIG_SOC_V32F20X_CPU0)
    struct metering_data md = {0};
#endif
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
	#if defined(CONFIG_SOC_V32F20X_CPU0)
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
