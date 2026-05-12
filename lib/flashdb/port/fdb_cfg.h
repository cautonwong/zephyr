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

/* Use KVDB and TSDB */
#define FDB_USING_KVDB
#define FDB_USING_TSDB

/* Use FAL mode */
#define FDB_USING_FAL_MODE

#endif /* _FDB_CFG_H_ */
