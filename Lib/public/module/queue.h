#pragma once

#ifndef LIBMODULE_STRUCT_H
    #define LIBMODULE_STRUCT_H
#endif
#include "cmn.h"

/** Queue interface **/

/* Callback for queue_iterate */
typedef int (*m_queue_cb)(void *, void *);

/* Fn for queue_set_dtor */
typedef void (*m_queue_dtor)(void *);

/* Incomplete struct declaration for queue */
typedef struct _queue m_queue_t;

/* Incomplete struct declaration for queue iterator */
typedef struct _queue_itr m_queue_itr_t;

m_queue_t *m_queue_new(m_queue_dtor fn);
m_queue_itr_t *m_queue_itr_new(const m_queue_t *q);
int m_queue_itr_next(m_queue_itr_t **itr);
int m_queue_itr_remove(m_queue_itr_t *itr);
void *m_queue_itr_get_data(const m_queue_itr_t *itr);
int m_queue_itr_set_data(const m_queue_itr_t *itr, void *value);
int m_queue_iterate(const m_queue_t *q, m_queue_cb fn, void *userptr);
int m_queue_enqueue(m_queue_t *q, void *data);
void *m_queue_dequeue(m_queue_t *q);
void *m_queue_peek(const m_queue_t *q);
int m_queue_remove(m_queue_t *q);
int m_queue_clear(m_queue_t *q);
int m_queue_free(m_queue_t **q);
ssize_t m_queue_len(const m_queue_t *q);
