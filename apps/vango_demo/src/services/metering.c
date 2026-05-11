/*
 * Metering Service - Hardware Abstracted Sampling
 */
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app_metering, LOG_LEVEL_INF);

#define ADC_NODE DT_ALIAS(app_adc)

void metering_service_run(void)
{
#if DT_NODE_EXISTS(ADC_NODE)
    const struct device *adc = DEVICE_DT_GET(ADC_NODE);

    if (!device_is_ready(adc)) {
        LOG_ERR("Metering Hardware (app-adc) not ready!");
        return;
    }

    LOG_INF("Metering Service Started using %s", adc->name);
    
    while (1) {
        /* Implementation of sampling logic using adc_read() */
        LOG_DBG("Sampling energy data...");
        k_msleep(1000);
    }
#else
    LOG_WRN("No 'app-adc' alias defined. Metering service suspended.");
#endif
}

K_THREAD_DEFINE(metering_tid, 1024, metering_service_run, NULL, NULL, NULL, 5, 0, 0);
