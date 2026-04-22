/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_DRIVERS_ADC_AD7616_H_
#define ZEPHYR_DRIVERS_ADC_AD7616_H_

#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>

/* AD7616 Register Map & Bit Definitions */
#define AD7616_REG_CONFIG                0x02
#define AD7616_REG_CHANNEL               0x03
#define AD7616_REG_RANGE_A1              0x04
#define AD7616_REG_RANGE_A2              0x05
#define AD7616_REG_RANGE_B1              0x06
#define AD7616_REG_RANGE_B2              0x07
#define AD7616_REG_SEQ_STACK_BASE        0x20

#define AD7616_SPI_RW_WRITE              BIT(15)
#define AD7616_SPI_REGADDR_POS           9

struct ad7616_data {
    const struct device *dev;
    struct k_sem acq_sem;
    struct gpio_callback busy_cb;
    
    /* DMA-Safe Persistence Layer */
    struct spi_buf rx_spi_buf;
    struct spi_buf_set rx_spi_set;
    struct adc_sequence *current_seq;
    
    uint16_t tx_cmd_buf; /* For register writes or read requests */
    struct spi_buf tx_spi_buf;
    struct spi_buf_set tx_spi_set;

    atomic_t state;
};

enum ad7616_state {
    AD7616_STATE_IDLE,
    AD7616_STATE_CONVERTING,
    AD7616_STATE_READING,
};

#endif /* ZEPHYR_DRIVERS_ADC_AD7616_H_ */
