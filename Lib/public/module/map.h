#pragma once

#ifndef LIBMODULE_STRUCT_H
    #define LIBMODULE_STRUCT_H
#endif
#include "cmn.h"

/** Hashmap interface **/

/* Callback for map_iterate; first parameter is userdata, second and third are {key,value} tuple */
typedef int (*m_map_cb)(void *, const char *, void *);

/* Fn for map_set_dtor */
typedef void (*m_map_dtor)(void *);

/* Incomplete struct declaration for hashmap */
typedef struct _map m_map_t;

/* Incomplete struct declaration for hashmap iterator */
typedef struct _map_itr m_map_itr_t;

typedef enum {
    M_MAP_KEY_DUP           = 0x01,         // Should map keys be dupped?
    M_MAP_KEY_AUTOFREE      = 0x02,         // Should map keys be freed automatically?
    M_MAP_VAL_ALLOW_UPDATE  = 0X04          // Does map object allow for updating values?
} m_map_flags;

m_map_t *m_map_new(m_map_flags flags, m_map_dtor fn);
m_map_itr_t *m_map_itr_new(const m_map_t *m);
int m_map_itr_next(m_map_itr_t **itr);
int m_map_itr_remove(m_map_itr_t *itr);
const char *m_map_itr_get_key(const m_map_itr_t *itr);
void *m_map_itr_get_data(const m_map_itr_t *itr);
int m_map_itr_set_data(const m_map_itr_t *itr, void *value);
int m_map_iterate(m_map_t *m, m_map_cb fn, void *userptr);
int m_map_put(m_map_t *m, const char *key, void *value);
void *m_map_get(const m_map_t *m, const char *key);
bool m_map_contains(const m_map_t *m, const char *key);
int m_map_remove(m_map_t *m, const char *key);
int m_map_clear(m_map_t *m);
int m_map_free(m_map_t **m);
ssize_t m_map_len(const m_map_t *m);
