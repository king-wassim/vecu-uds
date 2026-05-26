#ifndef VECU_APP_ENGINE_H
#define VECU_APP_ENGINE_H

/*
 * app_engine — fake "running engine" telemetry source.
 *
 * Drives three values exposed as UDS DIDs:
 *   - 0x010C : Engine RPM           (2 bytes, formula (A*256+B)/4)
 *   - 0x010D : Vehicle Speed        (1 byte, km/h)
 *   - 0x0105 : Engine Coolant Temp  (1 byte, °C - 40)
 *
 * Values oscillate slowly using a simple sinusoidal model so that
 * `read DID 010C` returned multiple times gives changing readings —
 * proves the simulation is actually running.
 *
 * Thread-safety: a single mutex protects the shared struct. Writes
 * happen on the app thread (app_engine_tick), reads on the dispatcher.
 */

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    pthread_mutex_t lock;
    uint16_t rpm;          /* raw 1/4 RPM units → published as RPM*4 */
    uint8_t  speed_kmh;
    uint8_t  coolant_c;    /* real °C */
    uint32_t tick;
} app_engine_t;

void app_engine_init(app_engine_t *a);
void app_engine_destroy(app_engine_t *a);

/* Advance simulation by one tick. Call ~10 Hz from the app thread. */
void app_engine_tick(app_engine_t *a);

/* DID read callbacks (registered in did_table at startup).
 * They use a module-static back-reference set by app_engine_install_did_callbacks. */
int app_read_rpm     (uint8_t *out, size_t cap, size_t *out_len);
int app_read_speed   (uint8_t *out, size_t cap, size_t *out_len);
int app_read_coolant (uint8_t *out, size_t cap, size_t *out_len);

void app_engine_install_did_callbacks(app_engine_t *a);

#endif /* VECU_APP_ENGINE_H */
