/*
 * WebAssembly (WAMR) Business Isolation Sandbox Service
 * Inspired by AkiraOS's lightweight security model.
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/ring_buffer.h>
#include <wasm_export.h>
#include <stdlib.h>
#include <string.h>

#include "wasm_sandbox.h"

LOG_MODULE_REGISTER(wasm_sandbox, LOG_LEVEL_INF);

/* 
 * Ring buffer for WASM sandbox messages to decouple execution from printing 
 * and avoid blocking the WASM thread.
 */
#define WASM_LOG_RING_BUF_SIZE 1024
RING_BUF_DECLARE(wasm_log_ring_buf, WASM_LOG_RING_BUF_SIZE);

/* WASM bytecode of our sandbox app (328 bytes) */
static const uint8_t wasm_app_bytecode[] = {
    0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x15, 0x04, 0x60, 0x02, 0x7f, 0x7f, 0x00, 
    0x60, 0x01, 0x7f, 0x01, 0x7f, 0x60, 0x00, 0x01, 0x7f, 0x60, 0x02, 0x7f, 0x7f, 0x01, 0x7f, 0x02, 
    0x29, 0x02, 0x03, 0x65, 0x6e, 0x76, 0x0b, 0x6c, 0x6f, 0x67, 0x5f, 0x74, 0x6f, 0x5f, 0x68, 0x6f, 
    0x73, 0x74, 0x00, 0x00, 0x03, 0x65, 0x6e, 0x76, 0x0f, 0x67, 0x65, 0x74, 0x5f, 0x73, 0x65, 0x6e, 
    0x73, 0x6f, 0x72, 0x5f, 0x64, 0x61, 0x74, 0x61, 0x00, 0x01, 0x03, 0x03, 0x02, 0x02, 0x03, 0x04, 
    0x05, 0x01, 0x70, 0x01, 0x01, 0x01, 0x05, 0x03, 0x01, 0x00, 0x02, 0x06, 0x08, 0x01, 0x7f, 0x01, 
    0x41, 0xa0, 0x88, 0x04, 0x0b, 0x07, 0x11, 0x02, 0x06, 0x6d, 0x65, 0x6d, 0x6f, 0x72, 0x79, 0x02, 
    0x00, 0x04, 0x6d, 0x61, 0x69, 0x6e, 0x00, 0x03, 0x0a, 0x23, 0x02, 0x18, 0x00, 0x41, 0x80, 0x88, 
    0x80, 0x80, 0x00, 0x41, 0x18, 0x10, 0x80, 0x80, 0x80, 0x80, 0x00, 0x41, 0x2a, 0x10, 0x81, 0x80, 0x80, 
    0x80, 0x00, 0x0b, 0x08, 0x00, 0x10, 0x82, 0x80, 0x80, 0x80, 0x00, 0x0b, 0x0b, 0x20, 0x01, 0x00, 0x41, 
    0x80, 0x08, 0x0b, 0x19, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x66, 0x72, 0x6f, 0x6d, 0x20, 0x57, 0x41, 
    0x53, 0x4d, 0x20, 0x53, 0x61, 0x6e, 0x64, 0x62, 0x6f, 0x78, 0x21, 0x00, 0x00, 0x5d, 0x04, 0x6e, 0x61, 
    0x6d, 0x65, 0x01, 0x36, 0x04, 0x00, 0x0b, 0x6c, 0x6f, 0x67, 0x5f, 0x74, 0x6f, 0x5f, 0x68, 0x6f, 0x73, 
    0x74, 0x01, 0x0f, 0x67, 0x65, 0x74, 0x5f, 0x73, 0x65, 0x6e, 0x73, 0x6f, 0x72, 0x5f, 0x64, 0x61, 0x74, 
    0x61, 0x02, 0x0f, 0x5f, 0x5f, 0x6f, 0x72, 0x69, 0x67, 0x69, 0x6e, 0x61, 0x6c, 0x5f, 0x6d, 0x61, 0x69, 
    0x6e, 0x03, 0x04, 0x6d, 0x61, 0x69, 0x6e, 0x07, 0x12, 0x01, 0x00, 0x0f, 0x5f, 0x5f, 0x73, 0x74, 0x61, 
    0x63, 0x6b, 0x5f, 0x70, 0x6f, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x09, 0x0a, 0x01, 0x00, 0x07, 0x2e, 0x72, 
    0x6f, 0x64, 0x61, 0x74, 0x61, 0x00, 0x38, 0x09, 0x70, 0x72, 0x6f, 0x64, 0x75, 0x63, 0x65, 0x72, 0x73, 
    0x01, 0x0c, 0x70, 0x72, 0x6f, 0x63, 0x65, 0x73, 0x73, 0x65, 0x64, 0x2d, 0x62, 0x79, 0x01, 0x0c, 0x55, 
    0x62, 0x75, 0x6e, 0x74, 0x75, 0x20, 0x63, 0x6c, 0x61, 0x6e, 0x67, 0x11, 0x31, 0x34, 0x2e, 0x30, 0x2e, 
    0x30, 0x2d, 0x31, 0x75, 0x62, 0x75, 0x6e, 0x74, 0x75, 0x31, 0x2e, 0x31
};

/* --- Native Imports Implementation --- */

/**
 * @brief Sandboxed print service exported to WASM applications.
 * Writes messages to an asynchronous ring buffer.
 */
static void wamr_log_to_host(wasm_exec_env_t exec_env, uint32_t msg_offset, uint32_t msg_len)
{
    wasm_module_inst_t inst = wasm_runtime_get_module_inst(exec_env);
    if (!inst) {
        LOG_ERR("log_to_host: Invalid module instance");
        return;
    }

    /* Verify the offset and size lie within the sandboxed WASM memory bounds */
    if (!wasm_runtime_validate_app_addr(inst, msg_offset, msg_len)) {
        LOG_ERR("WASM Sandbox security violation: out-of-bounds log access at offset 0x%x (size %u)", 
                msg_offset, msg_len);
        return;
    }

    /* Map WASM offset to native virtual memory address */
    const char *native_msg = (const char *)wasm_runtime_addr_app_to_native(inst, msg_offset);
    if (native_msg) {
        /* Atomic non-blocking push into Zephyr ring buffer to decouple logging */
        uint32_t written = ring_buf_put(&wasm_log_ring_buf, (const uint8_t *)native_msg, msg_len);
        if (written < msg_len) {
            LOG_WRN("WASM log ring buffer full! Lost %u bytes", msg_len - written);
        }
        
        /* Push a newline to separate log entries in the buffer */
        const uint8_t newline = '\n';
        ring_buf_put(&wasm_log_ring_buf, &newline, 1);
    }
}

/**
 * @brief Safe sensor query interface exported to WASM applications.
 */
static int wamr_get_sensor_data(wasm_exec_env_t exec_env, int sensor_id)
{
    LOG_INF("WASM app queried sensor ID: %d", sensor_id);
    
    /* Mock sensor read. In production, this pulls from global ZBUS or thread-safe cache. */
    int mock_value = 100 + sensor_id;
    return mock_value;
}

/* Native symbols mapping table */
static NativeSymbol native_symbols[] = {
    {
        .symbol = "log_to_host",
        .func_ptr = (void *)wamr_log_to_host,
        .signature = "(ii)", /* (msg_offset, msg_len) -> void */
        .attachment = NULL
    },
    {
        .symbol = "get_sensor_data",
        .func_ptr = (void *)wamr_get_sensor_data,
        .signature = "(i)i", /* (sensor_id) -> i32 */
        .attachment = NULL
    }
};

/* --- Runner Thread & Lifecycle Management --- */

#define WASM_THREAD_STACK_SIZE 16384
#define WASM_THREAD_PRIORITY 5
K_THREAD_STACK_DEFINE(wasm_thread_stack, WASM_THREAD_STACK_SIZE);
static struct k_thread wasm_thread;

static void wasm_sandbox_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    char error_buf[128] = {0};

    LOG_INF("Initializing WAMR isolation runtime (Alloc_With_Allocator)...");

    /* Step 1: Initialize WAMR with Standard Heap Allocator */
    RuntimeInitArgs init_args = {0};
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = malloc;
    init_args.mem_alloc_option.allocator.free_func = free;
    init_args.mem_alloc_option.allocator.realloc_func = realloc;

    if (!wasm_runtime_full_init(&init_args)) {
        LOG_ERR("WAMR runtime initialization failed");
        return;
    }

    /* Step 2: Register Native Imports Bridge */
    if (!wasm_runtime_register_natives("env", native_symbols, ARRAY_SIZE(native_symbols))) {
        LOG_ERR("Failed to register Native APIs in 'env' namespace");
        wasm_runtime_destroy();
        return;
    }

    LOG_INF("Loading embedded WebAssembly business module (%u bytes)...", (uint32_t)sizeof(wasm_app_bytecode));

    /* Step 3: Load the compiled bytecode */
    wasm_module_t module = wasm_runtime_load(wasm_app_bytecode, sizeof(wasm_app_bytecode), 
                                             error_buf, sizeof(error_buf));
    if (!module) {
        LOG_ERR("Failed to load WASM module: %s", error_buf);
        wasm_runtime_destroy();
        return;
    }

    /* Step 4: Instantiate the WASM module with a sandboxed heap & stack */
    uint32_t wasm_inst_heap_size = 16384;
    uint32_t wasm_inst_stack_size = 8192;
    wasm_module_inst_t inst = wasm_runtime_instantiate(module, wasm_inst_heap_size, wasm_inst_stack_size, 
                                                       error_buf, sizeof(error_buf));
    if (!inst) {
        LOG_ERR("Failed to instantiate WASM module: %s", error_buf);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return;
    }

    /* Step 5: Fetch WAMR Singleton Execution Environment */
    wasm_exec_env_t exec_env = wasm_runtime_get_exec_env_singleton(inst);
    if (!exec_env) {
        LOG_ERR("Failed to retrieve WAMR execution environment singleton");
        wasm_runtime_deinstantiate(inst);
        wasm_runtime_unload(module);
        wasm_runtime_destroy();
        return;
    }

    LOG_INF("Instantiated WASM sandbox successfully. Starting execution...");

    /* Step 6: Invoke WebAssembly entry point */
    bool success = wasm_application_execute_main(inst, 0, NULL);
    if (!success) {
        LOG_ERR("WASM Execution Exception occurred: %s", wasm_runtime_get_exception(inst));
    } else {
        LOG_INF("WASM sandbox main function returned execution status: SUCCESS");
    }

    /* Asynchronous log buffer flushing */
    uint8_t read_buf[64];
    uint32_t size;
    while ((size = ring_buf_get(&wasm_log_ring_buf, read_buf, sizeof(read_buf) - 1)) > 0) {
        read_buf[size] = '\0';
        printk("[WASM Log Channel] %s", (char *)read_buf);
    }

    /* Step 7: Release Resources */
    wasm_runtime_deinstantiate(inst);
    wasm_runtime_unload(module);
    wasm_runtime_destroy();

    LOG_INF("WAMR Sandbox runtime teardown completed.");
}

int wasm_sandbox_init(void)
{
    LOG_INF("Starting Lightweight WebAssembly Business Isolation Sandbox Thread...");
    
    k_thread_create(&wasm_thread, wasm_thread_stack,
                    K_THREAD_STACK_SIZEOF(wasm_thread_stack),
                    wasm_sandbox_thread_entry,
                    NULL, NULL, NULL,
                    WASM_THREAD_PRIORITY, 0, K_NO_WAIT);
                    
    k_thread_name_set(&wasm_thread, "wasm_sandbox");
    return 0;
}
