/*
 * FlashDB System Initialization
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <flashdb.h>

LOG_MODULE_REGISTER(fdb_init, LOG_LEVEL_INF);

#ifdef CONFIG_FLASHDB
struct fdb_kvdb kvdb1;
struct fdb_tsdb tsdb1;

static struct fdb_default_kv_node default_kv_table[] = {
    {"boot_count", "0", sizeof("0") - 1},
};

extern fdb_time_t fdb_get_time_impl(void);

static int storage_fdb_init(void)
{
    fdb_err_t result;
    struct fdb_default_kv default_kv;

    default_kv.kvs = default_kv_table;
    default_kv.num = sizeof(default_kv_table) / sizeof(default_kv_table[0]);

    /* 1. Initialize KVDB */
    result = fdb_kvdb_init(&kvdb1, "kvdb1", "storage", &default_kv, NULL);
    if (result != FDB_NO_ERR) {
        LOG_ERR("KVDB init failed: %d", result);
        return -EIO;
    }

    /* 2. Initialize TSDB */
    result = fdb_tsdb_init(&tsdb1, "tsdb1", "storage", fdb_get_time_impl, 128, NULL);
    if (result != FDB_NO_ERR) {
        LOG_ERR("TSDB init failed: %d", result);
        return -EIO;
    }

    LOG_INF("FlashDB initialized successfully via SYS_INIT");
    return 0;
}

/* 
 * Initialize at APPLICATION level, as it depends on the flash device 
 * and partitions being ready (POST_KERNEL).
 */
SYS_INIT(storage_fdb_init, APPLICATION, 90);
#endif
