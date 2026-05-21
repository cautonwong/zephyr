/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_ADC_AD7616_H_
#define ZEPHYR_DRIVERS_ADC_AD7616_H_

#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/kernel.h>

/* AD7616 Register Map */
#define AD7616_REG_CONFIG                0x02
#define AD7616_REG_CHANNEL               0x03
#define AD7616_REG_RANGE_A1              0x04
#define AD7616_REG_RANGE_A2              0x05
#define AD7616_REG_RANGE_B1              0x06
#define AD7616_REG_RANGE_B2              0x07
#define AD7616_REG_SEQ_STACK_BASE        0x20

/* Register Bit Definitions */
#define AD7616_SPI_RW_WRITE              BIT(15)
#define AD7616_SPI_REGADDR_POS           9
#define AD7616_SPI_REGADDR_MSK           GENMASK(14, 9)
#define AD7616_SPI_DATA_MSK              GENMASK(8, 0)

#define AD7616_CONFIG_BURSTEN            BIT(6)
#define AD7616_CONFIG_SEQEN              BIT(5)
#define AD7616_CONFIG_OS_POS             2
#define AD7616_CONFIG_OS_MSK             GENMASK(4, 2)

/* Range settings */
#define AD7616_RANGE_10V                 0x0
#define AD7616_RANGE_2V5                 0x1
#define AD7616_RANGE_5V                  0x2

/* Hardware state structure */
struct ad7616_data {
    const struct device *dev;
    struct k_sem acq_sem;
    struct k_mutex lock;
    struct gpio_callback busy_cb;
    
    /* DMA-Safe Persistence Layer */
    struct spi_buf rx_spi_buf;
    struct spi_buf_set rx_spi_set;
    const struct adc_sequence *current_seq;
    
    uint16_t rx_data_buf[16]; /* Max 16 channels */
    
    uint32_t active_channels;
    uint8_t step_count;
    uint16_t sample_count;
    uint16_t sample_limit;
    uint16_t *buffer_ptr;
    
    bool async;
    atomic_t state;
};

/* Device configuration */
struct ad7616_config {
    struct spi_dt_spec spi;
    struct gpio_dt_spec reset_gpio;
    struct gpio_dt_spec convst_gpio;
    struct gpio_dt_spec busy_gpio;
    struct pwm_dt_spec convst_pwm;
    uint8_t default_osr;
    bool sequencer_en;
};

#endif /* ZEPHYR_DRIVERS_ADC_AD7616_H_ */
DRIVERS_ADC_AD7616_H_ */
