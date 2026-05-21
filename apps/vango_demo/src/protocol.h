#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

struct metering_data {
    uint32_t active_energy;
    uint32_t reactive_energy;
    uint32_t timestamp;
};

#endif
