/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT analog_devices_ad7616

#include <zephyr/kernel.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

#include "ad7616.h"

LOG_MODULE_REGISTER(AD7616, CONFIG_ADC_LOG_LEVEL);

struct ad7616_config {
    struct spi_dt_spec spi;
    struct gpio_dt_spec reset_gpio;
    struct gpio_dt_spec convst_gpio;
    struct gpio_dt_spec busy_gpio;
    struct pwm_dt_spec convst_pwm; /* For 1MSPS hardware trigger */
};

/* --- Advanced Async/Streaming Logic --- */

/**
 * @brief SPI Async Callback (DMA complete)
 */
static void ad7616_spi_done_cb(const struct device *spi_dev, int error, void *cb_data)
{
    struct ad7616_data *data = (struct ad7616_data *)cb_data;

    /* For 1MSPS, we would typically swap buffers here (Ping-Pong)
     * and notify the application via a high-speed callback.
     */
    k_sem_give(&data->acq_sem);
}

/**
 * @brief BUSY Pin Falling Edge (Conversion Finished)
 * This is called every 1us in 1MSPS mode. 
 * ISR MUST BE EXTREMELY LEAN.
 */
static void ad7616_busy_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    struct ad7616_data *data = CONTAINER_OF(cb, struct ad7616_data, busy_cb);
    const struct ad7616_config *config = data->dev->config;

    /* Prepare DMA structures */
    data->rx_spi_buf.buf = data->current_seq->buffer;
    data->rx_spi_buf.len = data->current_seq->buffer_size;
    data->rx_spi_set.buffers = &data->rx_spi_buf;
    data->rx_spi_set.count = 1;

    /* Start DMA transfer. Bottom half will handle notification. */
    spi_transceive_cb(&config->spi, NULL, &data->rx_spi_set, ad7616_spi_done_cb, data);
}

/* --- ADC API --- */

static int ad7616_read(const struct device *dev, const struct adc_sequence *sequence)
{
    const struct ad7616_config *config = dev->config;
    struct ad7616_data *data = dev->data;

    data->current_seq = (struct adc_sequence *)sequence;
    k_sem_reset(&data->acq_sem);

    if (config->convst_pwm.dev != NULL) {
        /* 1MSPS MODE: Start PWM Hardware Trigger */
        pwm_set_dt(&config->convst_pwm, PWM_HZ(1000000), PWM_HZ(2000000));
    } else {
        /* SINGLE SHOT: Manual Trigger */
        gpio_pin_set_dt(&config->convst_gpio, 1);
        k_busy_wait(1);
        gpio_pin_set_dt(&config->convst_gpio, 0);
    }

    /* Wait for DMA to complete the requested sequence */
    return k_sem_take(&data->acq_sem, K_MSEC(100));
}

/* ... Standard boilerplate (init, instantiation) ... */

static int ad7616_init(const struct device *dev)
{
    const struct ad7616_config *config = dev->config;
    struct ad7616_data *data = dev->data;
    
    data->dev = dev;
    k_sem_init(&data->acq_sem, 0, 1);

    if (!spi_is_ready_dt(&config->spi)) return -ENODEV;

    gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_ACTIVE);
    
    /* Setup BUSY Interrupt for high-speed triggering of SPI DMA */
    gpio_pin_configure_dt(&config->busy_gpio, GPIO_INPUT);
    gpio_init_callback(&data->busy_cb, ad7616_busy_handler, BIT(config->busy_gpio.pin));
    gpio_add_callback(config->busy_gpio.port, &data->busy_cb);
    gpio_pin_interrupt_configure_dt(&config->busy_gpio, GPIO_INT_EDGE_TO_INACTIVE);

    if (config->convst_pwm.dev != NULL) {
        if (!pwm_is_ready_dt(&config->convst_pwm)) return -ENODEV;
    } else {
        gpio_pin_configure_dt(&config->convst_gpio, GPIO_OUTPUT_INACTIVE);
    }

    /* HW Reset */
    gpio_pin_set_dt(&config->reset_gpio, 0);
    k_msleep(1);
    gpio_pin_set_dt(&config->reset_gpio, 1);
    k_msleep(15);

    LOG_INF("AD7616 Pro-Driver: 1MSPS capable via PWM Trigger + SPI DMA");
    return 0;
}

#define AD7616_INST(n)                                                         \
    static struct ad7616_data ad7616_data_##n;                                 \
    static const struct ad7616_config ad7616_config_##n = {                    \
        .spi = SPI_DT_SPEC_INST_GET(n, SPI_OP_MODE_MASTER | SPI_WORD_SET(16), 0), \
        .reset_gpio = GPIO_DT_SPEC_INST_GET(n, reset_gpios),                   \
        .convst_gpio = GPIO_DT_SPEC_INST_GET_OR(n, convst_gpios, {0}),         \
        .convst_pwm = PWM_DT_SPEC_INST_GET_OR(n, pwms, {0}),                   \
        .busy_gpio = GPIO_DT_SPEC_INST_GET(n, busy_gpios),                     \
    };                                                                         \
    DEVICE_DT_INST_DEFINE(n, ad7616_init, NULL, &ad7616_data_##n,              \
                          &ad7616_config_##n, POST_KERNEL,                     \
                          40, &ad7616_api);

DT_INST_FOREACH_STATUS_OKAY(AD7616_INST)
