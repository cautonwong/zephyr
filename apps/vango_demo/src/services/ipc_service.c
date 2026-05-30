/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/ipc/ipc_service.h>
#include <zephyr/logging/log.h>
#include <protocol.h>

LOG_MODULE_REGISTER(ipc_svc, LOG_LEVEL_INF);

static void bound_cb(void *priv)
{
    LOG_INF("IPC Service bound successfully");
}

static void recv_cb(const void *data, size_t len, void *priv)
{
#if defined(CONFIG_SOC_V32F20X_CPUAPP)
    /* CPUAPP (Gateway) handles incoming metering data */
    if (len == sizeof(struct metering_data)) {
        const struct metering_data *md = (const struct metering_data *)data;
        LOG_INF("[CPUAPP] Received: Active=%u, Reactive=%u", md->active_energy, md->reactive_energy);
    }
#endif
}

static const struct device *ipc_instance;
static struct ipc_ept ept;

static struct ipc_ept_cfg ept_cfg = {
    .name = "vango_metering",
    .cb = {
        .bound = bound_cb,
        .received = recv_cb,
    },
};

int ipc_service_init(void)
{
#if DT_NODE_EXISTS(DT_NODELABEL(ipc0))
    int ret;

    ipc_instance = DEVICE_DT_GET(DT_NODELABEL(ipc0));
    if (!device_is_ready(ipc_instance)) {
        LOG_ERR("IPC instance not ready");
        return -ENODEV;
    }

    ret = ipc_service_register_endpoint(ipc_instance, &ept, &ept_cfg);
    if (ret < 0) {
        LOG_ERR("Failed to register endpoint: %d", ret);
        return ret;
    }
#else
    LOG_WRN("IPC hardware not found (Simulated/Mock environment)");
#endif
    return 0;
}

#if defined(CONFIG_SOC_V32F20X_CPUMETER)
int ipc_send_metering(struct metering_data *data)
{
    return ipc_service_send(&ept, data, sizeof(struct metering_data));
}
#endif
