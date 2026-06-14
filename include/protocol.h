/*
 * Copyright (c) 2026 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>

/* Professional Header for IPC Communication (TLV - Type-Length-Value) */
struct ipc_header {
    uint16_t magic;    /* 0x5A47 (VG) */
    uint8_t  type;     /* 1=Data, 2=Control, 3=Ack, 4=WaveformPtr */
    uint8_t  len;      /* Payload length */
} __attribute__((packed));

#define PROTOCOL_MAGIC 0x5A47

/* Type Definitions */
#define MSG_TYPE_METERING_DATA 1
#define MSG_TYPE_METERING_CTRL 2
#define MSG_TYPE_SYSTEM_EVENT  3
#define MSG_TYPE_WAVEFORM_PTR  4

/* Waveform Shared Memory Config (Aligned with app.overlay) */
#define WAVEFORM_SHM_BASE  0x20100000
#define WAVEFORM_SHM_SIZE  0x10000  /* 64KB */

/* Payload Structures */
struct metering_payload {
    uint32_t active_energy;
    uint32_t reactive_energy;
    uint32_t timestamp;
} __attribute__((packed));

struct control_payload {
    uint8_t  cmd;      /* 1=Reset, 2=SetFreq, 3=Calibration */
    uint32_t value;
} __attribute__((packed));

struct waveform_ptr_payload {
    uint32_t shm_address; /* Physical address in shared RAM */
    uint32_t sample_cnt;  /* Number of ADC samples */
    uint8_t  channel_mask;
} __attribute__((packed));

#endif
