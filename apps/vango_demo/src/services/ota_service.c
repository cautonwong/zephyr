/*
 * MCUboot & Secure OTA Readiness Simulator Service
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/storage/flash_map.h>
#include <zephyr/dfu/flash_img.h>
#include <zephyr/dfu/mcuboot.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(ota_svc, LOG_LEVEL_INF);

/* Custom sector size for simulated write blocks */
#define SIM_CHUNK_SIZE 1024
#define SIM_FIRMWARE_SIZE (256 * 1024) /* 256 KB simulated binary size */

static uint8_t simulated_firmware_chunk[SIM_CHUNK_SIZE];

/* --- Shell Commands --- */

static int cmd_ota_info(const struct shell *sh, size_t argc, char **argv)
{
    ARG_UNUSED(argc);
    ARG_UNUSED(argv);

    shell_print(sh, "MCUboot OTA Partition Mapping:");
    shell_print(sh, "--------------------------------");

    #if PARTITION_EXISTS(boot_partition)
    shell_print(sh, "Boot Slot (MCUboot) : ADDR 0x%08x | SIZE %d KB",
                PARTITION_OFFSET(boot_partition), PARTITION_SIZE(boot_partition) / 1024);
    #endif

    #if PARTITION_EXISTS(slot0_partition)
    shell_print(sh, "Primary Slot (slot0): ADDR 0x%08x | SIZE %d KB",
                PARTITION_OFFSET(slot0_partition), PARTITION_SIZE(slot0_partition) / 1024);
    #endif

    #if PARTITION_EXISTS(slot1_partition)
    shell_print(sh, "Secondary Slot (slot1): ADDR 0x%08x | SIZE %d KB",
                PARTITION_OFFSET(slot1_partition), PARTITION_SIZE(slot1_partition) / 1024);
    #endif

    #if PARTITION_EXISTS(scratch_partition)
    shell_print(sh, "Scratch Slot        : ADDR 0x%08x | SIZE %d KB",
                PARTITION_OFFSET(scratch_partition), PARTITION_SIZE(scratch_partition) / 1024);
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
    
    /* Initialize the flash image manager targeting slot 1 */
    ret = flash_img_init_id(&ctx, PARTITION_ID(slot1_partition));
    if (ret != 0) {
        shell_error(sh, "Failed to initialize flash image manager for slot1 (Error %d)", ret);
        return ret;
    }

    shell_print(sh, "Flash Image context initialized. Writing %d KB firmware binary...", SIM_FIRMWARE_SIZE / 1024);

    /* Generate and write dummy blocks of firmware */
    for (size_t offset = 0; offset < SIM_FIRMWARE_SIZE; offset += SIM_CHUNK_SIZE) {
        /* Populate dummy pattern in firmware buffer */
        for (size_t i = 0; i < SIM_CHUNK_SIZE; i++) {
            simulated_firmware_chunk[i] = (uint8_t)((offset + i) & 0xFF);
        }

        /* Write the block. Set flush to true on the last chunk */
        bool flush = (offset + SIM_CHUNK_SIZE >= SIM_FIRMWARE_SIZE);
        ret = flash_img_buffered_write(&ctx, simulated_firmware_chunk, SIM_CHUNK_SIZE, flush);
        if (ret != 0) {
            shell_error(sh, "Flash write failed at offset 0x%zx (Error %d)", offset, ret);
            return ret;
        }

        /* Update progress occasionally */
        if ((offset % (64 * 1024)) == 0) {
            shell_print(sh, "Downloaded and written: %d KB / %d KB...", 
                        (int)(offset + SIM_CHUNK_SIZE) / 1024, SIM_FIRMWARE_SIZE / 1024);
        }
    }

    shell_print(sh, "Simulated OTA Download COMPLETE!");
    shell_print(sh, "Requesting MCUboot upgrade for next boot (Upgrade Mode: TEST)...");

    /* Tell MCUboot that a new image is pending validation in slot1 */
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
    return 0;
}

int ota_service_init(void)
{
    LOG_INF("MCUboot OTA Readiness Service Initialized");
    return 0;
}

/* --- Shell Commands Registration --- */

SHELL_STATIC_SUBCMD_SET_CREATE(sub_ota,
    SHELL_CMD_ARG(info, NULL, "Show MCUboot partition mappings.", cmd_ota_info, 1, 0),
    SHELL_CMD_ARG(trigger_sim, NULL, "Simulate OTA download, flash, and trigger A/B swap reboot.", cmd_ota_trigger_sim, 1, 0),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(ota, &sub_ota, "MCUboot OTA upgrade commands", NULL);
