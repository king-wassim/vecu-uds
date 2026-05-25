#include "vecu.h"
#include <string.h>

void vecu_init(vecu_state_t *state) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->session = 0x01;
}
