#pragma once

#ifndef LIBMODULE_CORE_H
    #define LIBMODULE_CORE_H
#endif

#include "cmn.h"

/* Context flags */
typedef enum {
    M_CTX_NAME_DUP          = 0x01,         // Should ctx's name be strdupped? (only if a ctx is created during a register)
    M_CTX_NAME_AUTOFREE     = 0x02,         // Should ctx's name be autofreed? (only if a ctx is created during a register)
    M_CTX_PERSIST           = 0x04,         // Prevent ctx automatic destroying when there are no modules in it anymore. With this option, context is kept alive until m_ctx_deregister() is called.
    M_CTX_USERDATA_AUTOFREE = 0x08          // Automatically free ctx userdata upon deregister
} m_ctx_flags;

typedef struct {
    double activity_freq;
    uint64_t total_idle_time;
    uint64_t total_looping_time;
    uint64_t recv_msgs;
    size_t num_modules;
    size_t running_modules;
} m_ctx_stats_t;

/* Logger callback */
typedef void (*m_log_cb)(const m_mod_t *ref, const char *fmt, va_list args);

/* Context interface functions */
int m_ctx_register(const char *ctx_name, m_ctx_t **c, m_ctx_flags flags, const void *userdata);
int m_ctx_deregister(m_ctx_t **c);

int m_ctx_set_logger(m_ctx_t *c, m_log_cb logger);
int m_ctx_loop(m_ctx_t *c);
int m_ctx_quit(m_ctx_t *c, uint8_t quit_code);

int m_ctx_fd(const m_ctx_t *c);

int m_ctx_dispatch(m_ctx_t *c);

int m_ctx_dump(const m_ctx_t *c);
int m_ctx_stats(const m_ctx_t *c, m_ctx_stats_t *stats);

const char *m_ctx_name(const m_ctx_t *c);
const void *m_ctx_userdata(const m_ctx_t *c);