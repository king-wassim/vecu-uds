#ifndef VECU_H
#define VECU_H

#include "uds_types.h"

typedef struct {
    uint8_t session;
    uint8_t security_level;
} vecu_state_t;

void vecu_init(vecu_state_t *state);

#endif
