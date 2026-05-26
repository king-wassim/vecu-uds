#include "app_engine.h"

#include <math.h>
#include <string.h>

/*
 * Single back-reference so the DID read callbacks can find the state
 * without us threading a void* through the entire ReadDID call path.
 * The price: one app_engine_t per process. Acceptable: the ECU is one.
 */
static app_engine_t *g_app = NULL;

void app_engine_init(app_engine_t *a) {
    if (a == NULL) return;
    memset(a, 0, sizeof(*a));
    pthread_mutex_init(&a->lock, NULL);
    a->rpm       = 800 * 4;  /* idle, stored in 1/4 RPM units */
    a->speed_kmh = 0;
    a->coolant_c = 60;       /* warm-ish */
}

void app_engine_destroy(app_engine_t *a) {
    if (a == NULL) return;
    pthread_mutex_destroy(&a->lock);
}

void app_engine_install_did_callbacks(app_engine_t *a) {
    g_app = a;
}

/*
 * Drive RPM and speed with two slow sinusoids so the diag tool sees
 * something moving. Coolant climbs to 92 °C over the first ~5 minutes.
 * Step assumed ~100 ms (caller cadence).
 */
void app_engine_tick(app_engine_t *a) {
    if (a == NULL) return;
    pthread_mutex_lock(&a->lock);
    a->tick++;

    double t = (double)a->tick * 0.1;  /* seconds */

    double rpm   = 1500.0 + 700.0 * sin(t * 0.3);  /* 800..2200 RPM */
    double speed = 60.0   + 50.0  * sin(t * 0.15); /* 10..110 km/h  */

    a->rpm = (uint16_t)(rpm * 4.0);
    a->speed_kmh = (uint8_t)(speed < 0 ? 0 : (speed > 255 ? 255 : speed));

    if (a->coolant_c < 92) {
        /* +1 °C every 30 ticks (≈3 s) until 92 */
        if ((a->tick % 30) == 0) a->coolant_c++;
    }
    pthread_mutex_unlock(&a->lock);
}

int app_read_rpm(uint8_t *out, size_t cap, size_t *out_len) {
    if (g_app == NULL || out == NULL || out_len == NULL || cap < 2) return -1;
    pthread_mutex_lock(&g_app->lock);
    uint16_t rpm = g_app->rpm;
    pthread_mutex_unlock(&g_app->lock);
    out[0] = (uint8_t)(rpm >> 8);
    out[1] = (uint8_t)(rpm & 0xFF);
    *out_len = 2;
    return 0;
}

int app_read_speed(uint8_t *out, size_t cap, size_t *out_len) {
    if (g_app == NULL || out == NULL || out_len == NULL || cap < 1) return -1;
    pthread_mutex_lock(&g_app->lock);
    out[0] = g_app->speed_kmh;
    pthread_mutex_unlock(&g_app->lock);
    *out_len = 1;
    return 0;
}

int app_read_coolant(uint8_t *out, size_t cap, size_t *out_len) {
    if (g_app == NULL || out == NULL || out_len == NULL || cap < 1) return -1;
    pthread_mutex_lock(&g_app->lock);
    /* OBD encoding: raw byte = °C + 40. */
    int16_t enc = (int16_t)g_app->coolant_c + 40;
    if (enc < 0) enc = 0;
    if (enc > 255) enc = 255;
    out[0] = (uint8_t)enc;
    pthread_mutex_unlock(&g_app->lock);
    *out_len = 1;
    return 0;
}
