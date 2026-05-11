/*
 * Communication Service - High Performance Async IO
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_comms, LOG_LEVEL_INF);

#define UART_NODE DT_ALIAS(app_uart)

void comms_service_run(void)
{
#if DT_NODE_EXISTS(UART_NODE)
    const struct device *uart = DEVICE_DT_GET(UART_NODE);

    if (!device_is_ready(uart)) {
        LOG_ERR("Communication Hardware (app-uart) not ready!");
        return;
    }

    LOG_INF("Communication Service Started using %s", uart->name);

#if defined(CONFIG_UART_ASYNC_API)
    LOG_INF("DMA-based Async API enabled for %s", uart->name);
#endif

    while (1) {
        /* Heartbeat or Gateway logic */
        k_msleep(2000);
    }
#else
    LOG_WRN("No 'app-uart' alias defined. Communication service suspended.");
#endif
}

K_THREAD_DEFINE(comms_tid, 2048, comms_service_run, NULL, NULL, NULL, 7, 0, 0);
