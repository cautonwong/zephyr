/*
 * MCUboot & Secure OTA Service
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 *
 * Real janpatch delta OTA integration.
 */

#if defined(CONFIG_APP_FEATURE_OTA)

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/drivers/flash.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>
#include <janpatch.h>

#include <string.h>
#include <stdlib.h>

LOG_MODULE_REGISTER(ota_svc, LOG_LEVEL_INF);

#define SIM_CHUNK_SIZE 1024
#define SIM_FIRMWARE_SIZE (256 * 1024) /* 256 KB simulated binary size */
#define SIM_METER_SIZE    (64 * 1024)  /* 64 KB simulated M0 binary size */
#define PATCH_BUF_SIZE    (64 * 1024)  /* 64 KB max patch size */
#define MAX_SOURCE_SIZE   (384 * 1024) /* 384 KB max source image */

static uint8_t simulated_firmware_chunk[SIM_CHUNK_SIZE];

/* ------------------------------------------------------------------ */
/* janpatch streaming I/O helpers                                      */
/* ------------------------------------------------------------------ */

/* RAM-backed stream (for source and patch) */
struct ram_stream {
    const uint8_t *data;
    uint32_t size;
    uint32_t pos;
};

static int ram_read(void *buffer, uint32_t size, void *stream)
{
    struct ram_stream *s = (struct ram_stream *)stream;
    if (!s || s->pos + size > s->size) return -1;
    memcpy(buffer, s->data + s->pos, size);
    s->pos += size;
    return (int)size;
}

static int ram_seek(long offset, int origin, void *stream)
{
    struct ram_stream *s = (struct ram_stream *)stream;
    if (origin == 1) s->pos += offset; /* SEEK_CUR */
    return 0;
}

/* Flash-backed target writer (buffered through flash_img) */
struct flash_target {
    struct flash_img_context *ctx;
    uint8_t buf[SIM_CHUNK_SIZE];
    uint32_t pos;
};

static int flash_target_write(const void *buffer, uint32_t size, void *stream)
{
    struct flash_target *t = (struct flash_target *)stream;
    const uint8_t *src = (const uint8_t *)buffer;
    uint32_t remaining = size;

    while (remaining > 0) {
        uint32_t space = SIM_CHUNK_SIZE - t->pos;
        uint32_t copy = remaining < space ? remaining : space;
        memcpy(t->buf + t->pos, src, copy);
        t->pos += copy;
        src += copy;
        remaining -= copy;

        if (t->pos >= SIM_CHUNK_SIZE) {
            int ret = flash_img_buffered_write(t->ctx, t->buf, SIM_CHUNK_SIZE, false);
            if (ret != 0) return -1;
            t->pos = 0;
        }
    }
    return (int)size;
}

static int flash_target_flush(struct flash_target *t)
{
    if (t->pos == 0) return 0;
    int ret = flash_img_buffered_write(t->ctx, t->buf, t->pos, true);
    t->pos = 0;
    return ret;
}

/* ------------------------------------------------------------------ */
/* Patch buffer (loaded externally via shell or OTA download)          */
/* ------------------------------------------------------------------ */

static uint8_t patch_buffer[PATCH_BUF_SIZE];
static uint32_t patch_size;

/* ------------------------------------------------------------------ */
/* Shell commands                                                      */
/* ------------------------------------------------------------------ */

static int cmd_ota_info(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "MCUboot OTA Partition Mapping:");
    shell_print(sh, "--------------------------------");

    #if FIXED_PARTITION_EXISTS(boot_partition)
    shell_print(sh, "Boot Slot (MCUboot) : ADDR 0x%08x | SIZE %d KB",
                FIXED_PARTITION_OFFSET(boot_partition), FIXED_PARTITION_SIZE(boot_partition) / 1024);
    #endif

    #if FIXED_PARTITION_EXISTS(slot0_partition)
    shell_print(sh, "Primary Slot (slot0): ADDR 0x%08x | SIZE %d KB",
                FIXED_PARTITION_OFFSET(slot0_partition), FIXED_PARTITION_SIZE(slot0_partition) / 1024);
    #endif

    #if FIXED_PARTITION_EXISTS(slot1_partition)
    shell_print(sh, "Secondary Slot (slot1): ADDR 0x%08x | SIZE %d KB",
                FIXED_PARTITION_OFFSET(slot1_partition), FIXED_PARTITION_SIZE(slot1_partition) / 1024);
    #endif

    #if FIXED_PARTITION_EXISTS(scratch_partition)
    shell_print(sh, "Scratch Slot        : ADDR 0x%08x | SIZE %d KB",
                FIXED_PARTITION_OFFSET(scratch_partition), FIXED_PARTITION_SIZE(scratch_partition) / 1024);
    #endif

    #if FIXED_PARTITION_EXISTS(cpumeter_partition)
    shell_print(sh, "M0 Meter Slot       : ADDR 0x%08x | SIZE %d KB",
                FIXED_PARTITION_OFFSET(cpumeter_partition), FIXED_PARTITION_SIZE(cpumeter_partition) / 1024);
    #endif

    return 0;
}

static int cmd_ota_trigger_sim(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int ret;
    struct flash_img_context ctx;

    shell_print(sh, "Starting simulated OTA Firmware download to Slot 1...");

    #if FIXED_PARTITION_EXISTS(slot1_partition)
    ret = flash_img_init_id(&ctx, FIXED_PARTITION_ID(slot1_partition));
    if (ret != 0) {
        shell_error(sh, "Failed to initialize flash image manager for slot1 (Error %d)", ret);
        return ret;
    }

    shell_print(sh, "Flash Image context initialized. Writing %d KB firmware binary...", SIM_FIRMWARE_SIZE / 1024);

    for (size_t offset = 0; offset < SIM_FIRMWARE_SIZE; offset += SIM_CHUNK_SIZE) {
        for (size_t i = 0; i < SIM_CHUNK_SIZE; i++) {
            simulated_firmware_chunk[i] = (uint8_t)((offset + i) & 0xFF);
        }
        bool flush = (offset + SIM_CHUNK_SIZE >= SIM_FIRMWARE_SIZE);
        ret = flash_img_buffered_write(&ctx, simulated_firmware_chunk, SIM_CHUNK_SIZE, flush);
        if (ret != 0) {
            shell_error(sh, "Flash write failed at offset 0x%zx (Error %d)", offset, ret);
            return ret;
        }
        if ((offset % (64 * 1024)) == 0) {
            shell_print(sh, "Downloaded and written: %d KB / %d KB...",
                        (int)(offset + SIM_CHUNK_SIZE) / 1024, SIM_FIRMWARE_SIZE / 1024);
        }
    }

    shell_print(sh, "Simulated OTA Download COMPLETE!");
    shell_print(sh, "Requesting MCUboot upgrade for next boot...");

#ifdef CONFIG_BOOTLOADER_MCUBOOT
    ret = boot_request_upgrade(BOOT_UPGRADE_TEST);
    if (ret != 0) {
        shell_error(sh, "Failed to request MCUboot upgrade swap (Error %d)", ret);
        return ret;
    }
#else
    shell_print(sh, "Mocking MCUboot upgrade swap (MCUboot missing from workspace).");
#endif

    shell_print(sh, "Upgrade requested successfully! Rebooting system in 3 seconds to complete swap...");
    k_sleep(K_SECONDS(3));
    sys_reboot(SYS_REBOOT_WARM);
    #else
    shell_error(sh, "Slot 1 not found in devicetree.");
    #endif
    return 0;
}

static int cmd_ota_delta_apply(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    int ret;

    if (patch_size == 0) {
        shell_error(sh, "No patch loaded. Use 'ota patch_load <hex>' first.");
        return -ENOENT;
    }

    shell_print(sh, "Initiating Delta OTA Reconstruction...");

    /* 1. Open slot0 partition */
    const struct flash_area *fa_slot0;
    ret = flash_area_open(FIXED_PARTITION_ID(slot0_partition), &fa_slot0);
    if (ret != 0) {
        shell_error(sh, "Failed to open slot0 partition (Error %d)", ret);
        return ret;
    }

    uint32_t slot0_size = fa_slot0->fa_size;
    if (slot0_size > MAX_SOURCE_SIZE) slot0_size = MAX_SOURCE_SIZE;
    shell_print(sh, "1. Slot 0 opened: %d KB", (int)(slot0_size / 1024));

    /* 2. Pre-read slot0 into RAM */
    uint8_t *source_ram = (uint8_t *)k_malloc(slot0_size);
    if (!source_ram) {
        shell_error(sh, "Failed to allocate %d KB RAM for source", (int)(slot0_size / 1024));
        flash_area_close(fa_slot0);
        return -ENOMEM;
    }
    ret = flash_area_read(fa_slot0, 0, source_ram, slot0_size);
    if (ret != 0) {
        shell_error(sh, "Failed to read slot0 (Error %d)", ret);
        k_free(source_ram);
        flash_area_close(fa_slot0);
        return ret;
    }
    flash_area_close(fa_slot0);
    shell_print(sh, "2. Slot 0 loaded into RAM. Patch size: %d KB", (int)(patch_size / 1024));

    /* 3. Initialize slot1 target writer */
    struct flash_img_context img_ctx;
    ret = flash_img_init_id(&img_ctx, FIXED_PARTITION_ID(slot1_partition));
    if (ret != 0) {
        shell_error(sh, "Failed to init slot1 writer (Error %d)", ret);
        k_free(source_ram);
        return ret;
    }

    /* 4. Set up janpatch streams */
    struct ram_stream src = { .data = source_ram, .size = slot0_size, .pos = 0 };
    struct ram_stream pch = { .data = patch_buffer, .size = patch_size, .pos = 0 };
    struct flash_target tgt = { .ctx = &img_ctx, .pos = 0 };

    janpatch_ctx_t jctx = {
        .read  = ram_read,
        .write = flash_target_write,
        .seek  = ram_seek,
    };

    shell_print(sh, "3. Running janpatch engine...");

    ret = janpatch(jctx, &src, &pch, &tgt);
    if (ret != 0) {
        shell_error(sh, "janpatch failed (Error %d)", ret);
        flash_target_flush(&tgt);
        k_free(source_ram);
        return ret;
    }

    ret = flash_target_flush(&tgt);
    if (ret != 0) {
        shell_error(sh, "Failed to finalize slot1 write (Error %d)", ret);
        k_free(source_ram);
        return ret;
    }

    k_free(source_ram);
    shell_print(sh, "4. Delta synthesis complete! Firmware written to Slot 1.");

    /* 5. Request MCUboot swap */
#ifdef CONFIG_BOOTLOADER_MCUBOOT
    ret = boot_request_upgrade(BOOT_UPGRADE_TEST);
    if (ret != 0) {
        shell_error(sh, "Failed to request MCUboot upgrade (Error %d)", ret);
        return ret;
    }
    shell_print(sh, "5. MCUboot upgrade requested. Rebooting to apply delta...");
    k_sleep(K_SECONDS(1));
    sys_reboot(SYS_REBOOT_WARM);
#else
    shell_print(sh, "5. Delta applied. (MCUboot not present, skipping swap).");
#endif

    return 0;
}

static int cmd_ota_patch_load(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(sh, "Usage: ota patch_load <hex_bytes>");
        return -EINVAL;
    }

    const char *hex = argv[1];
    size_t hex_len = strlen(hex);
    size_t max_bytes = hex_len / 2;
    if (max_bytes > PATCH_BUF_SIZE) {
        shell_error(sh, "Patch too large: %zu bytes (max %d)", max_bytes, PATCH_BUF_SIZE);
        return -ENOMEM;
    }

    patch_size = 0;
    for (size_t i = 0; i + 1 < hex_len; i += 2) {
        char byte_str[3] = {hex[i], hex[i + 1], '\0'};
        char *end;
        patch_buffer[patch_size++] = (uint8_t)strtol(byte_str, &end, 16);
    }

    shell_print(sh, "Patch loaded: %d bytes (%.1f KB)", patch_size, patch_size / 1024.0f);
    return 0;
}

static int cmd_ota_meter_update(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "Initiating M0 Core (cpumeter) Hot Update Simulation...");

    #if FIXED_PARTITION_EXISTS(cpumeter_partition)
    int ret;
    struct flash_img_context ctx;

    ret = flash_img_init_id(&ctx, FIXED_PARTITION_ID(cpumeter_partition));
    if (ret != 0) {
        shell_error(sh, "M0 Partition Init Failed (Error %d)", ret);
        return ret;
    }

    shell_print(sh, "1. Writing M0 firmware to cpumeter partition...");
    for (size_t offset = 0; offset < SIM_METER_SIZE; offset += SIM_CHUNK_SIZE) {
        for (size_t i = 0; i < SIM_CHUNK_SIZE; i++) {
            simulated_firmware_chunk[i] = (uint8_t)((offset + i) ^ 0xAA);
        }
        bool flush = (offset + SIM_CHUNK_SIZE >= SIM_METER_SIZE);
        ret = flash_img_buffered_write(&ctx, simulated_firmware_chunk, SIM_CHUNK_SIZE, flush);
        if (ret != 0) {
            shell_error(sh, "M0 Flash Write Failed (0x%zx)", offset);
            return ret;
        }
    }

    shell_print(sh, "2. Triggering M0 core reset and hot reload...");
    extern void boot_cpu1_reload(void);
    boot_cpu1_reload();

    shell_print(sh, "3. M0 Hot Update COMPLETE! Main system running.");
    #else
    shell_error(sh, "M0 Partition NOT DEFINED in devicetree.");
    #endif

    return 0;
}

int ota_service_init(void)
{
    LOG_INF("MCUboot OTA Service Initialized (Delta & Hot-Update Enabled)");
    return 0;
}

/* --- Shell Commands Registration --- */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ota,
    SHELL_CMD_ARG(info, NULL, "Show MCUboot partition mappings.", cmd_ota_info, 1, 0),
    SHELL_CMD_ARG(trigger_sim, NULL, "Simulate OTA download, flash, and trigger A/B swap reboot.", cmd_ota_trigger_sim, 1, 0),
    SHELL_CMD_ARG(delta_apply, NULL, "Apply binary patch via janpatch to reconstruct new firmware in slot1.", cmd_ota_delta_apply, 1, 0),
    SHELL_CMD_ARG(patch_load, NULL, "Load a hex-encoded delta patch into RAM buffer.", cmd_ota_patch_load, 1, 0),
    SHELL_CMD_ARG(meter_update, NULL, "Hot-update the metering core firmware.", cmd_ota_meter_update, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ota, &sub_ota, "MCUboot OTA upgrade commands", NULL);

#endif /* CONFIG_APP_FEATURE_OTA */
