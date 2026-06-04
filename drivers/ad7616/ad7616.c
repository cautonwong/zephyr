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

/* --- Low Level SPI Register Access --- */

static int ad7616_reg_write(const struct device *dev, uint8_t reg, uint16_t val)
{
    const struct ad7616_config *config = dev->config;
    int ret;
    uint16_t tx_cmd = sys_cpu_to_be16(AD7616_SPI_RW_WRITE | 
                                      ((reg << AD7616_SPI_REGADDR_POS) & AD7616_SPI_REGADDR_MSK) | 
                                      (val & AD7616_SPI_DATA_MSK));

    struct spi_buf tx_buf = {
        .buf = &tx_cmd,
        .len = sizeof(tx_cmd)
    };
    struct spi_buf_set tx_set = {
        .buffers = &tx_buf,
        .count = 1
    };

    ret = spi_write_dt(&config->spi, &tx_set);
    if (ret < 0) {
        LOG_ERR("SPI write error: %d", ret);
    }
    return ret;
}

static int ad7616_reg_read(const struct device *dev, uint8_t reg, uint16_t *val)
{
    const struct ad7616_config *config = dev->config;
    int ret;
    uint16_t tx_cmd = sys_cpu_to_be16((reg << AD7616_SPI_REGADDR_POS) & AD7616_SPI_REGADDR_MSK);
    uint16_t rx_data;

    struct spi_buf tx_buf = {
        .buf = &tx_cmd,
        .len = sizeof(tx_cmd)
    };
    struct spi_buf_set tx_set = {
        .buffers = &tx_buf,
        .count = 1
    };

    /* AD7616 requires a dummy read cycle to fetch the requested register */
    ret = spi_write_dt(&config->spi, &tx_set);
    if (ret < 0) return ret;

    struct spi_buf rx_buf = {
        .buf = &rx_data,
        .len = sizeof(rx_data)
    };
    struct spi_buf_set rx_set = {
        .buffers = &rx_buf,
        .count = 1
    };

    /* Reading out the actual register value requested above */
    ret = spi_read_dt(&config->spi, &rx_set);
    if (ret < 0) return ret;

    *val = sys_be16_to_cpu(rx_data) & AD7616_SPI_DATA_MSK;
    return 0;
}

/* --- ADC Driver API Implementation --- */

static int ad7616_channel_setup(const struct device *dev,
                                const struct adc_channel_cfg *channel_cfg)
{
    uint8_t channel_id = channel_cfg->channel_id;
    uint8_t range_reg;
    uint8_t range_val;
    uint16_t current_range;
    int ret;

    if (channel_id > 15) {
        LOG_ERR("Invalid channel %d", channel_id);
        return -EINVAL;
    }

    if (channel_cfg->acquisition_time != ADC_ACQ_TIME_DEFAULT) {
        LOG_ERR("Custom acquisition time not supported");
        return -EINVAL;
    }

    if (channel_cfg->differential) {
        LOG_ERR("Differential mode configured in hardware, software toggle not supported");
        return -EINVAL;
    }

    /* Map Gain to Range (Example simplified mapping) */
    switch (channel_cfg->gain) {
    case ADC_GAIN_1:
        range_val = AD7616_RANGE_10V;
        break;
    case ADC_GAIN_2:
        range_val = AD7616_RANGE_5V;
        break;
    case ADC_GAIN_4:
        range_val = AD7616_RANGE_2V5;
        break;
    default:
        LOG_ERR("Unsupported gain %d", channel_cfg->gain);
        return -EINVAL;
    }

    /* Determine which register to write based on channel ID (A0-A3, A4-A7, B0-B3, B4-B7) */
    if (channel_id < 4) {
        range_reg = AD7616_REG_RANGE_A1;
    } else if (channel_id < 8) {
        range_reg = AD7616_REG_RANGE_A2;
        channel_id -= 4;
    } else if (channel_id < 12) {
        range_reg = AD7616_REG_RANGE_B1;
        channel_id -= 8;
    } else {
        range_reg = AD7616_REG_RANGE_B2;
        channel_id -= 12;
    }

    /* Read-Modify-Write Range Register */
    ret = ad7616_reg_read(dev, range_reg, &current_range);
    if (ret) return ret;

    current_range &= ~(0x3 << (channel_id * 2));
    current_range |= (range_val << (channel_id * 2));

    ret = ad7616_reg_write(dev, range_reg, current_range);
    if (ret) return ret;

    LOG_DBG("Configured Ch %d to Range %d", channel_cfg->channel_id, range_val);
    return 0;
}

static int ad7616_start_read(const struct device *dev,
                             const struct adc_sequence *sequence,
                             bool async)
{
    const struct ad7616_config *config = dev->config;
    struct ad7616_data *data = dev->data;
    int ret;

    if (sequence->resolution != 16) {
        LOG_ERR("Only 16-bit resolution supported");
        return -EINVAL;
    }

    if (sequence->channels == 0) {
        return -EINVAL;
    }

    k_mutex_lock(&data->lock, K_FOREVER);

    data->current_seq = sequence;
    data->async = async;
    data->active_channels = sequence->channels;
    data->buffer_ptr = (uint16_t *)sequence->buffer;
    data->sample_limit = sequence->buffer_size / sizeof(uint16_t);
    data->sample_count = 0;

    /* Disable BUSY interrupt during configuration */
    gpio_pin_interrupt_configure_dt(&config->busy_gpio, GPIO_INT_DISABLE);

    /* Map Zephyr channels (0-15) to AD7616 Sequencer Stack (pairs) */
    data->step_count = 0;
    uint8_t pairs_to_sample[8]; 
    for (int i = 0; i < 8; i++) {
        if ((data->active_channels & BIT(i)) || (data->active_channels & BIT(i + 8))) {
            pairs_to_sample[data->step_count] = i;
            data->step_count++;
        }
    }

    /* 1. Program Sequencer Stack */
    for (int k = 0; k < data->step_count; k++) {
        /* BSELx = i, ASELx = i */
        uint16_t stack_val = (pairs_to_sample[k] << 4) | (pairs_to_sample[k]); 
        if (k == data->step_count - 1) {
            stack_val |= AD7616_SEQ_STACK_SSREN; /* Set SSRENx in the last step */
        }
        ret = ad7616_reg_write(dev, AD7616_REG_SEQ_STACK_BASE + k, stack_val);
        if (ret) goto out;
    }

    /* 2. Enable BURSTEN and SEQEN in Config Register */
    uint16_t config_reg;
    ret = ad7616_reg_read(dev, AD7616_REG_CONFIG, &config_reg);
    config_reg |= (AD7616_CONFIG_BURSTEN | AD7616_CONFIG_SEQEN);
    ret = ad7616_reg_write(dev, AD7616_REG_CONFIG, config_reg);

    /* 3. Provide a dummy CONVST pulse to load the sequence */
    gpio_pin_set_dt(&config->convst_gpio, 1);
    k_busy_wait(1);
    gpio_pin_set_dt(&config->convst_gpio, 0);

    /* Wait for the dummy conversion to finish by polling BUSY */
    while (gpio_pin_get_dt(&config->busy_gpio) == 1) {
        k_busy_wait(1);
    }
    
    /* Re-enable BUSY interrupt for actual data capture */
    k_sem_reset(&data->acq_sem);
    gpio_pin_interrupt_configure_dt(&config->busy_gpio, GPIO_INT_EDGE_TO_INACTIVE);

    /* 4. Trigger actual sequence conversion */
    if (config->convst_pwm.dev != NULL) {
        /* 1MSPS MODE: Start PWM Hardware Trigger for CONVST */
        ret = pwm_set_dt(&config->convst_pwm, PWM_NSEC(1000), PWM_NSEC(500));
        if (ret) goto out;
    } else {
        /* SINGLE SHOT: Manual Trigger */
        gpio_pin_set_dt(&config->convst_gpio, 1);
        k_busy_wait(1);
        gpio_pin_set_dt(&config->convst_gpio, 0);
    }

    if (!async) {
        ret = k_sem_take(&data->acq_sem, K_MSEC(1000));
        if (ret == -EAGAIN) {
            LOG_ERR("ADC read timeout");
        }
        if (config->convst_pwm.dev != NULL) {
            pwm_set_dt(&config->convst_pwm, 0, 0);
        }
    }

out:
    if (!async || ret != 0) {
        k_mutex_unlock(&data->lock);
    }
    return ret;
}

static int ad7616_read_api(const struct device *dev,
                           const struct adc_sequence *sequence)
{
    return ad7616_start_read(dev, sequence, false);
}

#ifdef CONFIG_ADC_ASYNC
static int ad7616_read_async_api(const struct device *dev,
                                 const struct adc_sequence *sequence,
                                 struct k_poll_signal *async)
{
    int ret = ad7616_start_read(dev, sequence, true);
    if (ret == 0 && async) {
        k_poll_signal_raise(async, 0);
    }
    return ret;
}
#endif

/* --- Interrupt and DMA Handlers --- */

static void ad7616_spi_done_cb(const struct device *spi_dev, int error, void *cb_data)
{
    struct ad7616_data *data = (struct ad7616_data *)cb_data;
    
    if (error) {
        LOG_ERR("SPI Transfer failed: %d", error);
    }

    /* Extract data and move to application buffer */
    uint32_t channels = data->active_channels;
    uint8_t idx = 0;
    
    for (int i = 0; i < 16; i++) {
        if (channels & BIT(i)) {
            if (data->sample_count < data->sample_limit) {
                /* Swap endianness from SPI format */
                data->buffer_ptr[data->sample_count++] = sys_be16_to_cpu(data->rx_data_buf[idx++]);
            }
        }
    }

    /* If we have read all requested samples or doing a single shot */
    if (data->sample_count >= data->sample_limit || data->current_seq->options == NULL) {
        k_sem_give(&data->acq_sem);
    }
}

/**
 * @brief BUSY Pin Falling Edge (Conversion Finished)
 * Triggers SPI read to fetch converted data.
 */
static void ad7616_busy_handler(const struct device *port, struct gpio_callback *cb, uint32_t pins)
{
    struct ad7616_data *data = CONTAINER_OF(cb, struct ad7616_data, busy_cb);
    const struct ad7616_config *config = data->dev->config;

    /* Count number of active channels to read */
    uint32_t active_cnt = POPCOUNT(data->active_channels);
    if (active_cnt == 0) return;

    /* Prepare DMA SPI read */
    data->rx_spi_buf.buf = data->rx_data_buf;
    data->rx_spi_buf.len = active_cnt * sizeof(uint16_t);
    data->rx_spi_set.buffers = &data->rx_spi_buf;
    data->rx_spi_set.count = 1;

    /* Perform SPI Transceive asynchronously (DMA driven) */
    spi_read_cb(&config->spi, &data->rx_spi_set, ad7616_spi_done_cb, data);
}

#include <zephyr/pm/device.h>

/* --- Power Management Support --- */

#ifdef CONFIG_PM_DEVICE
static int ad7616_pm_action(const struct device *dev,
                            enum pm_device_action action)
{
    const struct ad7616_config *config = dev->config;
    int ret = 0;

    switch (action) {
    case PM_DEVICE_ACTION_SUSPEND:
        LOG_INF("Suspending AD7616: Putting chip in hardware reset");
        /* Pull reset low to minimize leakage current on the chip */
        gpio_pin_set_dt(&config->reset_gpio, 0);
        /* Note: SPI bus will be handled by its own PM if enabled */
        break;
    case PM_DEVICE_ACTION_RESUME:
        LOG_INF("Resuming AD7616: Re-initializing...");
        gpio_pin_set_dt(&config->reset_gpio, 1);
        k_msleep(15);
        /* Re-apply config if state was lost */
        break;
    default:
        return -ENOTSUP;
    }

    return ret;
}
#endif

/* --- Initialization --- */

static const struct adc_driver_api ad7616_api = {
    .channel_setup = ad7616_channel_setup,
    .read = ad7616_read_api,
#ifdef CONFIG_ADC_ASYNC
    .read_async = ad7616_read_async_api,
#endif
    .ref_internal = 2500, /* 2.5V Internal Ref */
};

static int ad7616_init(const struct device *dev)
{
    const struct ad7616_config *config = dev->config;
    struct ad7616_data *data = dev->data;
    
    data->dev = dev;
    k_mutex_init(&data->lock);
    k_sem_init(&data->acq_sem, 0, 1);

    if (!spi_is_ready_dt(&config->spi)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&config->reset_gpio)) {
        LOG_ERR("Reset GPIO not ready");
        return -ENODEV;
    }

    if (!gpio_is_ready_dt(&config->busy_gpio)) {
        LOG_ERR("Busy GPIO not ready");
        return -ENODEV;
    }

    gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_ACTIVE);
    
    /* Setup BUSY Interrupt */
    gpio_pin_configure_dt(&config->busy_gpio, GPIO_INPUT);
    gpio_init_callback(&data->busy_cb, ad7616_busy_handler, BIT(config->busy_gpio.pin));
    gpio_add_callback(config->busy_gpio.port, &data->busy_cb);
    gpio_pin_interrupt_configure_dt(&config->busy_gpio, GPIO_INT_EDGE_TO_INACTIVE);

    if (config->convst_pwm.dev != NULL) {
        if (!pwm_is_ready_dt(&config->convst_pwm)) {
            LOG_ERR("PWM device not ready");
            return -ENODEV;
        }
    } else if (config->convst_gpio.port != NULL) {
        if (!gpio_is_ready_dt(&config->convst_gpio)) {
            LOG_ERR("Convst GPIO not ready");
            return -ENODEV;
        }
        gpio_pin_configure_dt(&config->convst_gpio, GPIO_OUTPUT_INACTIVE);
    }

    /* Hardware Reset Sequence */
    gpio_pin_set_dt(&config->reset_gpio, 0);
    k_msleep(1);
    gpio_pin_set_dt(&config->reset_gpio, 1);
    k_msleep(15); /* Wait for initialization */

    /* Apply initial configuration (OSR, Burst mode, Sequencer) */
    uint16_t config_reg = 0;
    if (config->sequencer_en) {
        config_reg |= AD7616_CONFIG_SEQEN;
    }
    /* Map OSR (0, 2, 4, 8, 16, 32, 64, 128) to register (0 to 7) */
    uint8_t osr_val = 0;
    switch (config->default_osr) {
        case 2: osr_val = 1; break;
        case 4: osr_val = 2; break;
        case 8: osr_val = 3; break;
        case 16: osr_val = 4; break;
        case 32: osr_val = 5; break;
        case 64: osr_val = 6; break;
        case 128: osr_val = 7; break;
        default: osr_val = 0; break;
    }
    config_reg |= (osr_val << AD7616_CONFIG_OS_POS) & AD7616_CONFIG_OS_MSK;

    ad7616_reg_write(dev, AD7616_REG_CONFIG, config_reg);

    /* Start in Active mode by default */
    pm_device_runtime_enable(dev);

    LOG_INF("AD7616 initialized successfully");
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
        .default_osr = DT_INST_PROP_OR(n, adi_osr, 0),                         \
        .sequencer_en = DT_INST_PROP_OR(n, adi_sequencer_en, false),           \
    };                                                                         \
    PM_DEVICE_DT_INST_DEFINE(n, ad7616_pm_action);                             \
    DEVICE_DT_INST_DEFINE(n, ad7616_init, PM_DEVICE_DT_INST_GET(n),            \
                          &ad7616_data_##n, &ad7616_config_##n, POST_KERNEL,   \
                          CONFIG_ADC_INIT_PRIORITY, &ad7616_api);

DT_INST_FOREACH_STATUS_OKAY(AD7616_INST)
