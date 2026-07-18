/*
 * janpatch Unit Test
 * Tests delta patching algorithm directly
 */

#include <zephyr/ztest.h>
#include <janpatch.h>

/* RAM-based streaming helpers for janpatch */
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

static int ram_write(const void *buffer, uint32_t size, void *stream)
{
    struct ram_stream *s = (struct ram_stream *)stream;
    if (!s || s->pos + size > s->size) return -1;
    memcpy((uint8_t *)s->data + s->pos, buffer, size);
    s->pos += size;
    return (int)size;
}

static int ram_seek(long offset, int origin, void *stream)
{
    struct ram_stream *s = (struct ram_stream *)stream;
    if (origin == 1) s->pos += offset; /* SEEK_CUR */
    return 0;
}

/* Helper: run a janpatch round-trip test */
static int apply_patch(uint8_t *source, uint32_t source_size,
                       uint8_t *patch, uint32_t patch_size,
                       uint8_t *target, uint32_t target_size)
{
    struct ram_stream src = { .data = source, .size = source_size, .pos = 0 };
    struct ram_stream pch = { .data = patch,  .size = patch_size,  .pos = 0 };
    struct ram_stream tgt = { .data = target, .size = target_size, .pos = 0 };

    janpatch_ctx_t ctx = {
        .read  = ram_read,
        .write = ram_write,
        .seek  = ram_seek,
    };

    return janpatch(ctx, &src, &pch, &tgt);
}

/* Build a janpatch control block: [diff_len, extra_len, seek_offset] (3 x uint32_t LE) */
static void build_control(uint8_t *buf, uint32_t diff_len, uint32_t extra_len, int32_t seek_offset)
{
    buf[0] = diff_len & 0xFF;  buf[1] = (diff_len >> 8) & 0xFF;
    buf[2] = (diff_len >> 16) & 0xFF; buf[3] = (diff_len >> 24) & 0xFF;
    buf[4] = extra_len & 0xFF; buf[5] = (extra_len >> 8) & 0xFF;
    buf[6] = (extra_len >> 16) & 0xFF; buf[7] = (extra_len >> 24) & 0xFF;
    buf[8] = seek_offset & 0xFF; buf[9] = (seek_offset >> 8) & 0xFF;
    buf[10] = (seek_offset >> 16) & 0xFF; buf[11] = (seek_offset >> 24) & 0xFF;
}

ZTEST_SUITE(janpatch_suite, NULL, NULL, NULL, NULL, NULL);

/* Test: no-op patch (diff=0, extra=0, seek=0) reproduces source unchanged */
ZTEST(janpatch_suite, test_noop_patch)
{
    uint8_t source[] = {0x10, 0x20, 0x30, 0x40};
    uint8_t target[4] = {0};
    uint8_t patch[12];

    build_control(patch, 0, 0, 0);

    int ret = apply_patch(source, sizeof(source), patch, sizeof(patch), target, sizeof(target));
    zassert_equal(ret, 0, "no-op janpatch should return 0");

    /* Target should be empty (no diff or extra bytes) */
    zassert_equal(target[0], 0, "no-op target[0] should remain 0");
}

/* Test: diff-only patch (add 1 to each source byte) */
ZTEST(janpatch_suite, test_diff_patch)
{
    uint8_t source[] = {10, 20, 30, 40};
    uint8_t target[4] = {0};
    uint8_t patch[12 + 4]; /* control + 4 diff bytes */

    build_control(patch, 4, 0, 0);
    /* diff bytes: +1 to each */
    patch[12] = 1; patch[13] = 1; patch[14] = 1; patch[15] = 1;

    int ret = apply_patch(source, sizeof(source), patch, sizeof(patch), target, sizeof(target));
    zassert_equal(ret, 0, "diff janpatch should return 0");
    zassert_equal(target[0], 11, "target[0] should be 11");
    zassert_equal(target[1], 21, "target[1] should be 21");
    zassert_equal(target[2], 31, "target[2] should be 31");
    zassert_equal(target[3], 41, "target[3] should be 41");
}

/* Test: extra bytes appended after diff */
ZTEST(janpatch_suite, test_extra_patch)
{
    uint8_t source[] = {10, 20};
    uint8_t target[5] = {0};
    uint8_t patch[12 + 0 + 3]; /* control + 0 diff + 3 extra */

    build_control(patch, 0, 3, 0);
    /* extra bytes: 0xAA, 0xBB, 0xCC */
    patch[12] = 0xAA; patch[13] = 0xBB; patch[14] = 0xCC;

    int ret = apply_patch(source, sizeof(source), patch, sizeof(patch), target, sizeof(target));
    zassert_equal(ret, 0, "extra janpatch should return 0");
    zassert_equal(target[0], 0xAA, "target[0] should be 0xAA (extra)");
    zassert_equal(target[2], 0xCC, "target[2] should be 0xCC (extra)");
}

/* Test: source seek (skip bytes in source) */
ZTEST(janpatch_suite, test_seek_patch)
{
    uint8_t source[] = {0xFF, 0xFF, 10, 20, 30, 0xFF};
    uint8_t target[3] = {0};
    uint8_t patch[12 + 3]; /* control + 3 diff bytes */

    build_control(patch, 3, 0, 3);
    /* diff bytes: +1 to each */
    patch[12] = 1; patch[13] = 1; patch[14] = 1;

    int ret = apply_patch(source, sizeof(source), patch, sizeof(patch), target, sizeof(target));
    zassert_equal(ret, 0, "seek janpatch should return 0");
    /* Source was seeked by 3, so we read bytes 3,4,5 which are 10,20,30 -> 11,21,31 */
    zassert_equal(target[0], 11, "target[0] should be 11");
    zassert_equal(target[2], 31, "target[2] should be 31");
}

/* Test: full firmware reconstruction simulation */
ZTEST(janpatch_suite, test_firmware_reconstruct)
{
    /* Simulate 256-byte firmware delta */
    uint8_t source[256];
    uint8_t target[256];
    uint8_t patch[12 + 256]; /* control + patch data */
    uint32_t patch_pos = 0;

    /* Fill source with pattern */
    for (int i = 0; i < 256; i++) source[i] = (uint8_t)i;

    /* Build patch: diff first 128 bytes (+10 each), extra last 128 bytes */
    build_control(patch, 128, 128, 0);
    patch_pos = 12;
    /* Diff bytes: +10 to match source up to 128 */
    for (int i = 0; i < 128; i++) patch[patch_pos++] = 10;
    /* Extra bytes: new values for 128-255 */
    for (int i = 128; i < 256; i++) patch[patch_pos++] = (uint8_t)(i + 20);

    memset(target, 0, sizeof(target));
    int ret = apply_patch(source, sizeof(source), patch, patch_pos, target, sizeof(target));
    zassert_equal(ret, 0, "firmware reconstruct should return 0");

    /* Verify: first 128 bytes = source + 10 */
    for (int i = 0; i < 128; i++) {
        zassert_equal(target[i], i + 10, "byte %d should be %d", i, i + 10);
    }
    /* Verify: last 128 bytes = extra bytes */
    for (int i = 128; i < 256; i++) {
        zassert_equal(target[i], i + 20, "byte %d should be %d", i, i + 20);
    }
}
