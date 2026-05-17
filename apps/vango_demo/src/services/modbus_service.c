/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/modbus/modbus.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(modbus_svc, LOG_LEVEL_INF);

#define SLAVE_ADDR 1
#define POLL_INTERVAL K_SECONDS(5)

static void modbus_poll_task(void)
{
    int ret;
    uint16_t regs[4];
    int client_iface;

    const struct modbus_iface_param param = {
        .mode = MODBUS_MODE_RTU,
        .rx_timeout = 500000,
        .serial = {
            .baud = 115200,
            .parity = UART_CFG_PARITY_NONE,
            .stop_bits = UART_CFG_STOP_BITS_1,
        }
    };

    /* Use string name to get interface to avoid DTS ORD issues in some backend versions */
    client_iface = modbus_iface_get_by_name("modbus0");
    if (client_iface < 0) {
        LOG_ERR("Modbus interface 'modbus0' not found");
        return;
    }

    ret = modbus_init_client(client_iface, param);
    if (ret != 0) {
        LOG_ERR("Failed to init Modbus client: %d", ret);
        return;
    }

    LOG_INF("Modbus service started on modbus0");

    while (1) {
        ret = modbus_read_holding_regs(client_iface, SLAVE_ADDR, 0, regs, ARRAY_SIZE(regs));
        if (ret == 0) {
            LOG_INF("[MODBUS] Read: Reg0=%u, Reg1=%u", regs[0], regs[1]);
        } else {
            LOG_WRN("[MODBUS] Read failed: %d", ret);
        }
        k_sleep(POLL_INTERVAL);
    }
}

K_THREAD_DEFINE(modbus_thread, 2048, modbus_poll_task, NULL, NULL, NULL, 7, 0, 0);
