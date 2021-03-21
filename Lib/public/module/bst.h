#pragma once

#ifndef LIBMODULE_STRUCT_H
    #define LIBMODULE_STRUCT_H
#endif
#include "cmn.h"

/** Linked-List interface **/

/* Callback for tree_iterate; first parameter is userdata, second is tree data */
typedef int (*m_bst_cb)(void *, void *);

/* Fn for tree_set_dtor */
typedef void (*m_bst_dtor)(void *);

/* Callback for tree compare; first parameter is userdata, second is tree data */
typedef int (*m_bst_cmp)(void *, void *);

/* Incomplete struct declaration for tree */
typedef struct _bst m_bst_t;

/* Incomplete struct declaration for tree iterator */
typedef struct _bst_itr m_bst_itr_t;

typedef enum {
    M_BST_PRE,
    M_BST_POST,
    M_BST_IN
} m_bst_order;

m_bst_t *m_bst_new(m_bst_cmp comp, m_bst_dtor fn);
int m_bst_insert(m_bst_t *l, void *data);
int m_bst_remove(m_bst_t *l, void *data);
void *m_bst_find(m_bst_t *l, void *data);
int m_bst_iterate(m_bst_t *l, m_bst_cb cb, void *userptr);
int m_bst_traverse(m_bst_t *l, m_bst_order type, m_bst_cb cb, void *userptr);
m_bst_itr_t *m_bst_itr_new(const m_bst_t *l);
int m_bst_itr_next(m_bst_itr_t **itr);
int m_bst_itr_remove(m_bst_itr_t *itr);
void *m_bst_itr_get_data(const m_bst_itr_t *itr);
int m_bst_clear(m_bst_t *l);
int m_bst_free(m_bst_t **l);
ssize_t m_bst_len(const m_bst_t *l);


