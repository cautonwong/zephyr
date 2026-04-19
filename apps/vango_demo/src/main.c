/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(vango_demo, LOG_LEVEL_INF);

/* Global core identity */
#if defined(CONFIG_SOC_V32F20X_CPU0)
  #define CORE_TAG "[CM0]"
#else
  #define CORE_TAG "[CM33]"
#endif

/* Device Node Safe Access */
#define ADC_NODE DT_NODELABEL(adc0)
#define LED0_NODE DT_ALIAS(led0)

int main(void)
{
	LOG_INF("%s Professional Vango Demo starting...", CORE_TAG);

	/* 1. LED Heartbeat Initialization */
#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
	const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
	if (gpio_is_ready_dt(&led)) {
		gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
		LOG_INF("%s Heartbeat LED ready.", CORE_TAG);
	}
#else
	LOG_WRN("%s LED0 not enabled in devicetree.", CORE_TAG);
#endif

	/* 2. ADC Acquisition Logic (Task for CM0) */
#if defined(CONFIG_SOC_V32F20X_CPU0)
#if DT_NODE_HAS_STATUS_OKAY(ADC_NODE)
	const struct device *const adc_dev = DEVICE_DT_GET(ADC_NODE);
	if (!device_is_ready(adc_dev)) {
		LOG_ERR("%s ADC device not ready!", CORE_TAG);
	} else {
		LOG_INF("%s ADC Data Acquisition Core initialized.", CORE_TAG);
	}
#else
	LOG_ERR("%s ADC0 is disabled. Check app.overlay!", CORE_TAG);
#endif
#endif

	/* 3. Main Loop */
	uint32_t loop_cnt = 0;
	while (1) {
		LOG_INF("%s Uptime: %u s | Iteration: %u", 
			CORE_TAG, (uint32_t)(k_uptime_get() / 1000), loop_cnt++);

#if DT_NODE_HAS_STATUS_OKAY(LED0_NODE)
		gpio_pin_toggle_dt(&led);
#endif

		/* Simulated heavy work for CPU0 vs CPU1 */
#if defined(CONFIG_SOC_V32F20X_CPU0)
		k_msleep(1000); /* CM0 handles 1Hz sampling */
#else
		k_msleep(2000); /* CM33 handles 0.5Hz reporting */
#endif
	}

	return 0;
}
