/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_

#include <zephyr/kernel.h>

/* Use Zephyr logging for FlashDB */
#define FDB_PRINT(...) printk(__VA_ARGS__)

#ifdef CONFIG_FLASHDB_WRITE_GRAN
#define FDB_WRITE_GRAN CONFIG_FLASHDB_WRITE_GRAN
#else
#define FDB_WRITE_GRAN 32
#endif

/* Total number of sector store status. Vango Flash has 1024 bytes per page. */
#ifdef FDB_SECTOR_STORE_STATUS_NUM
#undef FDB_SECTOR_STORE_STATUS_NUM
#endif
#define FDB_SECTOR_STORE_STATUS_NUM 10

/* Total number of sector dirty status. */
#ifdef FDB_SECTOR_DIRTY_STATUS_NUM
#undef FDB_SECTOR_DIRTY_STATUS_NUM
#endif
#define FDB_SECTOR_DIRTY_STATUS_NUM 5

/* Use KVDB and TSDB */
#define FDB_USING_KVDB
#define FDB_USING_TSDB

/* Use FAL mode */
#define FDB_USING_FAL_MODE

#endif /* _FDB_CFG_H_ */
