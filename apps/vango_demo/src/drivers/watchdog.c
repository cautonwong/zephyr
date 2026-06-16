#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_wdt, LOG_LEVEL_INF);

#define WDT_NODE DT_ALIAS(watchdog0)

#if DT_NODE_HAS_STATUS(WDT_NODE, okay)
static const struct device *const wdt = DEVICE_DT_GET(WDT_NODE);
static int wdt_channel_id;
#endif

int watchdog_init(void)
{
#if DT_NODE_HAS_STATUS(WDT_NODE, okay)
    if (!device_is_ready(wdt)) {
        LOG_ERR("Watchdog device not ready");
        return -EIO;
    }

    struct wdt_timeout_cfg wdt_config = {
        .window.min = 0U,
        .window.max = 10000U, /* 10 seconds timeout */
        .callback = NULL,
        .flags = WDT_FLAG_RESET_SOC,
    };

    wdt_channel_id = wdt_install_timeout(wdt, &wdt_config);
    if (wdt_channel_id < 0) {
        LOG_ERR("Watchdog install timeout failed");
        return wdt_channel_id;
    }

    wdt_setup(wdt, WDT_OPT_PAUSE_IN_SLEEP);
    LOG_INF("Watchdog initialized (10s timeout)");
#else
    LOG_WRN("Watchdog not enabled for this core");
#endif
    return 0;
}

void watchdog_feed(void)
{
#if DT_NODE_HAS_STATUS(WDT_NODE, okay)
    wdt_feed(wdt, wdt_channel_id);
#endif
}
