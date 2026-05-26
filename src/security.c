#include "security.h"
#include "uds_types.h"
#include "vecu.h"

#include <string.h>
#include <sys/random.h>

#define SECURITY_KEY_CONSTANT 0xDEADBEEFu

void security_init(security_state_t *s) {
    if (s == NULL) return;
    memset(s, 0, sizeof(*s));
    pthread_mutex_init(&s->lock, NULL);
}

void security_destroy(security_state_t *s) {
    if (s == NULL) return;
    pthread_mutex_destroy(&s->lock);
}

/* Only level 1 is implemented for the demo. */
static bool level_supported(uint8_t level) {
    return level == 0x01;
}

int security_request_seed(security_state_t *s, uint8_t level,
                          uint32_t *out_seed, uint8_t *nrc) {
    if (s == NULL || out_seed == NULL || nrc == NULL) return -1;

    if (!level_supported(level)) {
        *nrc = UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
        return -1;
    }

    pthread_mutex_lock(&s->lock);

    /* Already unlocked at this level? Spec says respond with seed = 0. */
    if (s->unlocked_level >= level) {
        *out_seed = 0;
        s->seed_issued = false;
        pthread_mutex_unlock(&s->lock);
        return 1;
    }

    /* Still in lock-out window? */
    if (s->lock_until_ms != 0) {
        uint64_t now = vecu_now_ms();
        if (now < s->lock_until_ms) {
            *nrc = UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
            pthread_mutex_unlock(&s->lock);
            return -1;
        }
        /* Lock-out expired — reset fail counter. */
        s->lock_until_ms = 0;
        s->fail_count    = 0;
    }

    /* Pull 4 bytes of entropy. */
    uint32_t seed = 0;
    ssize_t got = getrandom(&seed, sizeof(seed), 0);
    if (got != (ssize_t)sizeof(seed)) {
        /* Fallback: never 0 (which would mean "already unlocked"). */
        seed = 0xA5A5A5A5u;
    }
    if (seed == 0) seed = 0xA5A5A5A5u;

    s->pending_seed = seed;
    s->seed_issued  = true;
    *out_seed = seed;

    pthread_mutex_unlock(&s->lock);
    return 0;
}

int security_send_key(security_state_t *s, uint8_t level,
                      uint32_t key, uint8_t *nrc) {
    if (s == NULL || nrc == NULL) return -1;

    if (!level_supported(level)) {
        *nrc = UDS_NRC_SUB_FUNCTION_NOT_SUPPORTED;
        return -1;
    }

    pthread_mutex_lock(&s->lock);

    /* No seed pending → sendKey out of sequence. */
    if (!s->seed_issued) {
        *nrc = UDS_NRC_REQUEST_SEQUENCE_ERROR;
        pthread_mutex_unlock(&s->lock);
        return -1;
    }

    /* Lock-out window check (defensive — should have been caught earlier). */
    if (s->lock_until_ms != 0 && vecu_now_ms() < s->lock_until_ms) {
        *nrc = UDS_NRC_REQUIRED_TIME_DELAY_NOT_EXPIRED;
        pthread_mutex_unlock(&s->lock);
        return -1;
    }

    uint32_t expected = s->pending_seed ^ SECURITY_KEY_CONSTANT;
    s->seed_issued = false;  /* one-shot, regardless of outcome */

    if (key == expected) {
        s->unlocked_level = level;
        s->fail_count = 0;
        pthread_mutex_unlock(&s->lock);
        return 0;
    }

    s->fail_count++;
    if (s->fail_count >= SECURITY_MAX_FAILS) {
        s->lock_until_ms = vecu_now_ms() + SECURITY_LOCK_DURATION_MS;
        *nrc = UDS_NRC_EXCEEDED_NUMBER_OF_ATTEMPTS;
    } else {
        *nrc = UDS_NRC_INVALID_KEY;
    }
    pthread_mutex_unlock(&s->lock);
    return -1;
}

bool security_is_unlocked(security_state_t *s, uint8_t level) {
    if (s == NULL) return false;
    pthread_mutex_lock(&s->lock);
    bool r = (s->unlocked_level >= level);
    pthread_mutex_unlock(&s->lock);
    return r;
}
