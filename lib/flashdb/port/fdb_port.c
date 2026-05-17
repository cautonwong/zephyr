/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <flashdb.h>
#include <fal.h>
#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/drivers/flash.h>
#include <string.h>

static struct k_mutex fdb_lock_obj;
static bool fdb_lock_init = false;

/* FAL bridge implementation */
static struct fal_partition zephyr_parts[4];
static int zephyr_part_count = 0;
static struct fal_flash_dev zephyr_flash_dev = {
    .name = "vango_flash",
    .blk_size = 1024
};

const struct fal_flash_dev *fal_flash_device_find(const char *name)
{
    return &zephyr_flash_dev;
}

const struct fal_partition *fal_partition_find(const char *name)
{
    for (int i = 0; i < zephyr_part_count; i++) {
        if (strcmp(zephyr_parts[i].name, name) == 0) {
            return &zephyr_parts[i];
        }
    }

    if (zephyr_part_count >= 4) return NULL;

    const struct flash_area *fa;
    int rc = -1;

    /* Use unified 'storage' partition for all FlashDB operations in production layout */
    rc = flash_area_open(DT_FIXED_PARTITION_ID(DT_NODELABEL(storage_partition)), &fa);

    if (rc == 0) {
        strncpy(zephyr_parts[zephyr_part_count].name, name, 15);
        strncpy(zephyr_parts[zephyr_part_count].flash_name, "vango_flash", 15);
        zephyr_parts[zephyr_part_count].fa = fa;
        zephyr_parts[zephyr_part_count].offset = 0;
        zephyr_parts[zephyr_part_count].len = fa->fa_size;
        return &zephyr_parts[zephyr_part_count++];
    }

    return NULL;
}

int fal_partition_read(const struct fal_partition *part, uint32_t addr, uint8_t *buf, size_t size)
{
    return flash_area_read(part->fa, addr, buf, size);
}

int fal_partition_write(const struct fal_partition *part, uint32_t addr, const uint8_t *buf, size_t size)
{
    return flash_area_write(part->fa, addr, buf, size);
}

int fal_partition_erase(const struct fal_partition *part, uint32_t addr, size_t size)
{
    return flash_area_erase(part->fa, addr, size);
}

/* FlashDB Porting */
void fdb_lock(fdb_db_t db)
{
    if (!fdb_lock_init) {
        k_mutex_init(&fdb_lock_obj);
        fdb_lock_init = true;
    }
    k_mutex_lock(&fdb_lock_obj, K_FOREVER);
}

void fdb_unlock(fdb_db_t db)
{
    k_mutex_unlock(&fdb_lock_obj);
}

/* FlashDB uses a function pointer in fdb_tsdb */
fdb_time_t fdb_get_time_impl(void)
{
    return (fdb_time_t)k_uptime_get() / 1000;
}
