/*
 * Copyright (c) 2024 Vango Technologies
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>

struct metering_data {
    uint32_t active_energy;
    uint32_t reactive_energy;
    uint32_t timestamp;
};

#endif
