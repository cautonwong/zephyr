/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(net_svc, LOG_LEVEL_INF);

static struct net_mgmt_event_callback mgmt_cb;

/* Note: Zephyr 4.4 uses uint64_t for mgmt_event in many platforms */
static void handler(struct net_mgmt_event_callback *cb,
                    uint64_t mgmt_event,
                    struct net_if *iface)
{
    if (mgmt_event == NET_EVENT_L4_CONNECTED) {
        LOG_INF("Network connected (L4)");
    } else if (mgmt_event == NET_EVENT_L4_DISCONNECTED) {
        LOG_INF("Network disconnected");
    }
}

static void network_monitor_task(void)
{
    LOG_INF("Network monitoring service started");

    net_mgmt_init_event_callback(&mgmt_cb, handler,
                                 NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED);
    net_mgmt_add_event_callback(&mgmt_cb);

    while (1) {
        struct net_if *iface = net_if_get_default();
        if (iface) {
            bool is_up = net_if_is_up(iface);
            LOG_DBG("Interface is %s", is_up ? "UP" : "DOWN");
        }
        k_sleep(K_SECONDS(30));
    }
}

K_THREAD_DEFINE(net_thread, 2048, network_monitor_task, NULL, NULL, NULL, 5, 0, 0);
