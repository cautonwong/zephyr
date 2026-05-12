/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FAL_H_
#define _FAL_H_

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>

/* Mocking FAL for FlashDB on Zephyr */

struct fal_flash_dev {
    char name[16];
    uint32_t blk_size;
};

struct fal_partition {
    char name[16];
    char flash_name[16];
    const struct flash_area *fa;
    uint32_t offset;
    uint32_t len;
};

static inline int fal_init(void) { return 0; }
const struct fal_flash_dev *fal_flash_device_find(const char *name);
const struct fal_partition *fal_partition_find(const char *name);
int fal_partition_read(const struct fal_partition *part, uint32_t addr, uint8_t *buf, size_t size);
int fal_partition_write(const struct fal_partition *part, uint32_t addr, const uint8_t *buf, size_t size);
int fal_partition_erase(const struct fal_partition *part, uint32_t addr, size_t size);

#endif /* _FAL_H_ */
