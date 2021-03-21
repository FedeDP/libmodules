#pragma once

#ifndef LIBMODULE_STRUCT_H
    #define LIBMODULE_STRUCT_H
#endif
#include "cmn.h"

/** Linked-List interface **/

/* Callback for list_iterate; first parameter is userdata, second is list data */
typedef int (*m_list_cb)(void *, void *);

/* Fn for list_set_dtor */
typedef void (*m_list_dtor)(void *);

/* Callback for list compare; first parameter is userdata, second is list data */
typedef int (*m_list_cmp)(void *, void *);

/* Incomplete struct declaration for list */
typedef struct _list m_list_t;

/* Incomplete struct declaration for list iterator */
typedef struct _list_itr m_list_itr_t;

m_list_t *m_list_new(m_list_cmp comp, m_list_dtor fn);
m_list_itr_t *m_list_itr_new(const m_list_t *l);
int m_list_itr_next(m_list_itr_t **itr);
void *m_list_itr_get_data(const m_list_itr_t *itr);
int m_list_itr_set_data(m_list_itr_t *itr, void *value);
int m_list_itr_insert(m_list_itr_t *itr, void *value);
int m_list_itr_remove(m_list_itr_t *itr);
int m_list_iterate(const m_list_t *l, m_list_cb fn, void *userptr);
int m_list_insert(m_list_t *l, void *data);
int m_list_remove(m_list_t *l, void *data);
void *m_list_find(m_list_t *l, void *data);
int m_list_clear(m_list_t *l);
int m_list_free(m_list_t **l);
ssize_t m_list_len(const m_list_t *l);

