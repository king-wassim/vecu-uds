#ifndef VECU_DID_TABLE_H
#define VECU_DID_TABLE_H

/*
 * DID (Data Identifier) table — central registry of values readable via
 * UDS service 0x22 ReadDataByIdentifier and writable via 0x2E.
 *
 * Two kinds of entries:
 *   - Static  : pointer + length, returned verbatim (e.g. VIN, ECU serial)
 *   - Dynamic : read/write callbacks (e.g. RPM, vehicle speed, owner name)
 *
 * The table is configured at vecu_init() and queried with did_table_lookup.
 * Storage for dynamic values lives in app_engine.c (telemetry) or in a
 * small RAM buffer for writable owner data — keeps did_table.c logic-free.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DID_TABLE_MAX 16

typedef int (*did_read_fn) (uint8_t *out, size_t out_cap, size_t *out_len);
typedef int (*did_write_fn)(const uint8_t *in, size_t len);

typedef struct {
    uint16_t        did;
    const uint8_t  *static_data;   /* NULL if dynamic */
    size_t          static_len;
    did_read_fn     read;          /* NULL if static-only */
    did_write_fn    write;         /* NULL if read-only */
    bool            requires_security;
    bool            requires_extended_session;
} did_entry_t;

typedef struct {
    did_entry_t entries[DID_TABLE_MAX];
    size_t      count;
} did_table_t;

void did_table_init(did_table_t *t);
int  did_table_add (did_table_t *t, const did_entry_t *entry);

/* Lookup. Returns pointer to entry or NULL. */
const did_entry_t *did_table_lookup(const did_table_t *t, uint16_t did);

#endif /* VECU_DID_TABLE_H */
