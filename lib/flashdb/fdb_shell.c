/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/shell/shell.h>
#include <flashdb.h>
#include <stdlib.h>
#include <string.h>

extern struct fdb_kvdb kvdb1;
extern struct fdb_tsdb tsdb1;

/* --- System Commands --- */
static int cmd_fdb_status(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "FlashDB Status:");
    shell_print(sh, "  KVDB '%s': %s", kvdb1.parent.name, kvdb1.parent.init_ok ? "Active" : "Error");
    shell_print(sh, "  TSDB '%s': %s", tsdb1.parent.name, tsdb1.parent.init_ok ? "Active" : "Error");
    return 0;
}

/* --- KVDB Commands --- */
static int cmd_kv_get(const struct shell *sh, size_t argc, char **argv)
{
    struct fdb_blob blob;
    char buf[128];
    size_t read_len;
    bool hex_mode = (argc > 2 && strcmp(argv[2], "-x") == 0);

    read_len = fdb_kv_get_blob(&kvdb1, argv[1], fdb_blob_make(&blob, buf, sizeof(buf)-1));
    if (read_len == 0) {
        shell_error(sh, "Key '%s' not found or empty", argv[1]);
        return -ENOENT;
    }

    if (hex_mode) {
        shell_hexdump(sh, (uint8_t *)buf, read_len);
    } else {
        buf[read_len] = '\0';
        shell_print(sh, "%s", buf);
    }
    return 0;
}

static int cmd_kv_set(const struct shell *sh, size_t argc, char **argv)
{
    fdb_err_t err = fdb_kv_set(&kvdb1, argv[1], argv[2]);
    if (err != FDB_NO_ERR) {
        shell_error(sh, "Failed to set KV (err: %d)", err);
        return -EIO;
    }
    shell_print(sh, "Done");
    return 0;
}

static int cmd_kv_del(const struct shell *sh, size_t argc, char **argv)
{
    fdb_err_t err = fdb_kv_del(&kvdb1, argv[1]);
    if (err != FDB_NO_ERR) {
        shell_error(sh, "Failed to delete key (err: %d)", err);
        return -EIO;
    }
    shell_print(sh, "Deleted");
    return 0;
}

static int cmd_kv_list(const struct shell *sh, size_t argc, char **argv)
{
    struct fdb_kv_iterator iterator;
    fdb_kv_t cur_kv;

    shell_print(sh, "Key              | Size | Status");
    shell_print(sh, "---------------------------------------");
    
    fdb_kv_iterator_init(&kvdb1, &iterator);
    while (fdb_kv_iterate(&kvdb1, &iterator)) {
        cur_kv = &(iterator.curr_kv);
        shell_print(sh, "  %-16s | len: %-4d | %s", 
                    cur_kv->name, cur_kv->len, 
                    cur_kv->crc_is_ok ? "CRC-OK" : "CRC-ERR");
    }
    return 0;
}

/* --- TSDB Commands --- */
static bool ts_list_cb(fdb_tsl_t tsl, void *arg)
{
    const struct shell *sh = (const struct shell *)arg;
    struct fdb_blob blob;
    int data = 0;

    fdb_blob_read((fdb_db_t)&tsdb1, fdb_tsl_to_blob(tsl, fdb_blob_make(&blob, &data, sizeof(data))));
    shell_print(sh, "[%10d] Data: %d", (int)tsl->time, data);
    return false;
}

static int cmd_ts_list(const struct shell *sh, size_t argc, char **argv)
{
    shell_print(sh, "Time (s)   | Value");
    shell_print(sh, "------------------");
    fdb_tsl_iter(&tsdb1, ts_list_cb, (void *)sh);
    return 0;
}

static int cmd_ts_clear(const struct shell *sh, size_t argc, char **argv)
{
    fdb_tsl_clean(&tsdb1);
    shell_print(sh, "TSDB Cleared");
    return 0;
}

#ifdef CONFIG_COREDUMP
extern void crash_test(void);
static int cmd_crash(const struct shell *sh, size_t argc, char **argv)
{
    crash_test();
    return 0;
}
#endif

/* --- Shell Registration --- */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_fdb_kv,
    SHELL_CMD_ARG(get,  NULL, "Get KV value: kv get <key> [-x]", cmd_kv_get, 2, 1),
    SHELL_CMD_ARG(set,  NULL, "Set KV value: kv set <key> <val>", cmd_kv_set, 3, 0),
    SHELL_CMD_ARG(del,  NULL, "Delete KV: kv del <key>", cmd_kv_del, 2, 0),
    SHELL_CMD(list,     NULL, "List all KVs", cmd_kv_list),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fdb_ts,
    SHELL_CMD(list,     NULL, "List all logs", cmd_ts_list),
    SHELL_CMD(clear,    NULL, "Clear all logs", cmd_ts_clear),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_fdb,
    SHELL_CMD(status,   NULL, "Show FlashDB status", cmd_fdb_status),
    SHELL_CMD(kv,       &sub_fdb_kv, "KVDB commands", NULL),
    SHELL_CMD(ts,       &sub_fdb_ts, "TSDB commands", NULL),
#ifdef CONFIG_COREDUMP
    SHELL_CMD(crash,    NULL, "Trigger intentional crash", cmd_crash),
#endif
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(fdb, &sub_fdb, "FlashDB commands", NULL);
