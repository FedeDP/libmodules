#pragma once

#ifndef LIBMODULE_TH_H
    #define LIBMODULE_TH_H
#endif
#include "cmn.h"
#include <pthread.h>

typedef void *(*m_thpool_task)(void *);

typedef struct _thpool m_thpool_t;

typedef enum {
    M_THPOOL_LAZY           = 0x01,         // lazy creation of threads
    M_THPOOL_DETACHED       = 0x02,         // create threads detached
} m_thpool_flags;


m_thpool_t *m_thpool_new(uint8_t thread_count, m_thpool_flags flags);
int m_thpool_add(m_thpool_t *pool, m_thpool_task task, void *arg);
ssize_t m_thpool_length(m_thpool_t *pool);
ssize_t m_thpool_clear(m_thpool_t *pool);
int m_thpool_free(m_thpool_t **pool, bool wait_all);
