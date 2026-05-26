#ifndef VECU_SECURITY_H
#define VECU_SECURITY_H

/*
 * SecurityAccess (UDS service 0x27) state machine.
 *
 * Seed/key protocol:
 *   1. Tester → ECU : 27 01            (requestSeed level 1)
 *   2. ECU → Tester : 67 01 <seed4>    (random 4-byte seed)
 *   3. Tester → ECU : 27 02 <key4>     (sendKey)
 *   4. ECU verifies. OK → 67 02, KO → NRC 0x35 invalidKey
 *
 * Anti-bruteforce: after MAX_FAILS wrong keys, the ECU refuses any
 * requestSeed for LOCK_DURATION_MS with NRC 0x37.
 *
 * Demo algorithm: key = seed XOR 0xDEADBEEF (replace by HSM in real life).
 */

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define SECURITY_MAX_FAILS         3
#define SECURITY_LOCK_DURATION_MS  10000

typedef struct {
    pthread_mutex_t lock;
    uint8_t  unlocked_level;    /* 0 = locked */
    uint32_t pending_seed;
    bool     seed_issued;
    uint8_t  fail_count;
    uint64_t lock_until_ms;     /* monotonic ms, 0 = not locked out */
} security_state_t;

void security_init(security_state_t *s);
void security_destroy(security_state_t *s);

/* Issue a seed for the given level. Returns:
 *   0 on success, *out_seed set
 *   1 if already unlocked at this level (seed = 0, per ISO 14229)
 *  -1 if level unsupported / temporarily locked out (set *nrc).        */
int security_request_seed(security_state_t *s, uint8_t level,
                          uint32_t *out_seed, uint8_t *nrc);

/* Verify a key for the given level. Returns:
 *   0 on success (unlocked)
 *  -1 on failure (set *nrc).                                            */
int security_send_key(security_state_t *s, uint8_t level,
                      uint32_t key, uint8_t *nrc);

bool security_is_unlocked(security_state_t *s, uint8_t level);

#endif /* VECU_SECURITY_H */
