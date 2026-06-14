/*
 * Copyright (c) 2026 Vango Technologies
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
    const char *name = (const char *)priv;
    LOG_INF("IPC Endpoint [%s] bound successfully", name);
}

static void data_recv_cb(const void *data, size_t len, void *priv)
{
#if defined(CONFIG_SOC_V32F20X_CPUAPP)
    /* CPUAPP (Gateway) handles incoming high-frequency metering data */
    const struct ipc_header *hdr = (const struct ipc_header *)data;
    if (len >= sizeof(struct ipc_header) && hdr->magic == PROTOCOL_MAGIC) {
        if (hdr->type == MSG_TYPE_METERING_DATA) {
            const struct metering_payload *p = (const struct metering_payload *)((uint8_t *)data + sizeof(struct ipc_header));
            LOG_INF("[M33] Data: Active=%u, Time=%u", p->active_energy, p->timestamp);
        } else if (hdr->type == MSG_TYPE_WAVEFORM_PTR) {
            const struct waveform_ptr_payload *p = (const struct waveform_ptr_payload *)((uint8_t *)data + sizeof(struct ipc_header));
            /* ZERO-COPY ACCESS: Read directly from physical SHM address */
            LOG_INF("[M33] Zero-copy waveform received at 0x%08x (Cnt: %u)", p->shm_address, p->sample_cnt);
            /* Access example: uint16_t *raw_data = (uint16_t *)(uintptr_t)p->shm_address; */
        }
    }
#endif
}

static void ctrl_recv_cb(const void *data, size_t len, void *priv)
{
    /* Handle control commands (e.g., Reset Metering, Change Sampling Rate) */
    LOG_INF("Received Control Message (Len: %d)", len);
}

static const struct device *ipc_instance;
static struct ipc_ept data_ept;
static struct ipc_ept ctrl_ept;

static struct ipc_ept_cfg data_ept_cfg = {
    .name = "metering-data",
    .cb = { .bound = bound_cb, .received = data_recv_cb },
    .priv = "data",
};

static struct ipc_ept_cfg ctrl_ept_cfg = {
    .name = "metering-ctrl",
    .cb = { .bound = bound_cb, .received = ctrl_recv_cb },
    .priv = "ctrl",
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

    /* Register Dual Endpoints for Professional Orchestration */
    ret = ipc_service_register_endpoint(ipc_instance, &data_ept, &data_ept_cfg);
    if (ret < 0) return ret;

    ret = ipc_service_register_endpoint(ipc_instance, &ctrl_ept, &ctrl_ept_cfg);
    if (ret < 0) return ret;

    LOG_INF("Standardized IPC Service (RPMsg) Initialized");
#else
    LOG_WRN("No IPC hardware found - Running in standalone mode");
#endif
    return 0;
}

#if defined(CONFIG_SOC_V32F20X_CPUMETER)
/* Cortex-M0 Side API: Send Metering Data */
int ipc_send_metering(struct metering_payload *payload)
{
    uint8_t buffer[sizeof(struct ipc_header) + sizeof(struct metering_payload)];
    struct ipc_header *hdr = (struct ipc_header *)buffer;

    hdr->magic = PROTOCOL_MAGIC;
    hdr->type = MSG_TYPE_METERING_DATA;
    hdr->len = sizeof(struct metering_payload);
    memcpy(buffer + sizeof(struct ipc_header), payload, sizeof(struct metering_payload));

    return ipc_service_send(&data_ept, buffer, sizeof(buffer));
}

/* Cortex-M0 Side API: Send Zero-copy Waveform Pointer */
int ipc_send_waveform_ptr(struct waveform_ptr_payload *payload)
{
    uint8_t buffer[sizeof(struct ipc_header) + sizeof(struct waveform_ptr_payload)];
    struct ipc_header *hdr = (struct ipc_header *)buffer;

    hdr->magic = PROTOCOL_MAGIC;
    hdr->type = MSG_TYPE_WAVEFORM_PTR;
    hdr->len = sizeof(struct waveform_ptr_payload);
    memcpy(buffer + sizeof(struct ipc_header), payload, sizeof(struct waveform_ptr_payload));

    return ipc_service_send(&data_ept, buffer, sizeof(buffer));
}
#endif
