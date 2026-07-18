/*
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 *
 * Lightweight Binary Patching Engine Interface
 */

#ifndef JANPATCH_H
#define JANPATCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Callback table for janpatch I/O operations */
typedef struct {
    int (*read)(void *buffer, uint32_t size, void *stream);
    int (*write)(const void *buffer, uint32_t size, void *stream);
    int (*seek)(long offset, int origin, void *stream);
} janpatch_ctx_t;

/**
 * @brief Apply a binary patch to reconstruct new firmware.
 *
 * @param ctx    I/O callback table
 * @param source Current firmware stream (read + seek)
 * @param patch  Delta patch stream (read only)
 * @param target Output firmware stream (write only)
 * @return 0 on success, -1 on error
 */
int janpatch(janpatch_ctx_t ctx, void *source, void *patch, void *target);

#ifdef __cplusplus
}
#endif

#endif /* JANPATCH_H */
