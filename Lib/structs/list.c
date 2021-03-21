#include "priv.h"

typedef struct _elem {
    void *userptr;
    struct _elem *next;
} list_node;

struct _list {
    size_t len;
    m_list_cmp comp;
    m_list_dtor dtor;
    list_node *data;
};

struct _list_itr {
    list_node **elem;
    m_list_t *l;
    ssize_t diff;
};

static inline int insert_node(m_list_t *l, list_node **elem, void *data);
static inline int remove_node(m_list_t *l, list_node **elem);

static inline int insert_node(m_list_t *l, list_node **elem, void *data) {
    list_node *node = memhook._malloc(sizeof(list_node));
    M_ALLOC_ASSERT(node);
    node->next = *elem;
    node->userptr = data;
    *elem = node;
    l->len++;
    return 0;
}

static inline int remove_node(m_list_t *l, list_node **elem) {
    list_node *tmp = *elem;
    if (tmp) {
        *elem = (*elem)->next;
        if (l->dtor) {
            l->dtor(tmp->userptr);
        }
        memhook._free(tmp);
        l->len--;
        return 0;
    }
    return -ENOENT;
}

/** Public API **/

_public_ m_list_t *m_list_new(m_list_cmp comp, m_list_dtor fn) {
    m_list_t *l = memhook._calloc(1, sizeof(m_list_t));
    if (l) {
        l->comp = comp;
        l->dtor = fn;
    }
    return l;
}

_public_ m_list_itr_t *m_list_itr_new(const m_list_t *l) {
    M_RET_ASSERT(m_list_len(l) > 0, NULL);
    
    m_list_itr_t *itr = memhook._calloc(1, sizeof(m_list_itr_t));
    if (itr) {
        itr->elem = (list_node **)&(l->data);
        itr->l = (m_list_t *)l;
    }
    return itr;
}

_public_ int m_list_itr_next(m_list_itr_t **itr) {
    M_PARAM_ASSERT(itr && *itr);
    
    m_list_itr_t *i = *itr;
    if (*i->elem) {
        if (i->diff >= 0) {
            i->elem = &((*i->elem)->next);
        } 
        i->diff = 0;
    }
    if (!*(i->elem)) {
        memhook._free(*itr);
        *itr = NULL;
    }
    return 0;
}

_public_ void *m_list_itr_get_data(const m_list_itr_t *itr) {
    M_RET_ASSERT(itr, NULL);
    M_RET_ASSERT(*itr->elem, NULL);
    
    return (*itr->elem)->userptr;
}

_public_ int m_list_itr_set_data(m_list_itr_t *itr, void *value) {
    M_PARAM_ASSERT(itr);
    M_PARAM_ASSERT(value);
    M_RET_ASSERT(*itr->elem, -EINVAL);
    
    (*itr->elem)->userptr = value;
    return 0;
}

_public_ int m_list_itr_insert(m_list_itr_t *itr, void *value) {
    M_PARAM_ASSERT(itr);
    M_PARAM_ASSERT(value);
    
    itr->diff++;
    return insert_node(itr->l, itr->elem, value);
}

_public_ int m_list_itr_remove(m_list_itr_t *itr) {
    M_PARAM_ASSERT(itr);
    M_RET_ASSERT(*itr->elem, -EINVAL);
    
    itr->diff--; // notify list to avoid skipping 1 element on next list_itr_next() call
    return remove_node(itr->l, itr->elem);
}

_public_ int m_list_iterate(const m_list_t *l, m_list_cb fn, void *userptr) {
    M_PARAM_ASSERT(fn);
    M_PARAM_ASSERT(m_list_len(l) > 0);
    
    list_node *elem = l->data;
    while (elem) {
        int rc = fn(userptr, elem->userptr);
        if (rc < 0) {
            /* Stop right now with error */
            return rc;
        }
        if (rc > 0) {
            /* Stop right now with 0 */
            return 0;
        }
        elem = elem->next;
    }
    return 0;
}

_public_ int m_list_insert(m_list_t *l, void *data) {
    M_PARAM_ASSERT(l);
    M_PARAM_ASSERT(data);
    
    list_node **tmp = &l->data;
    for (int i = 0; i < l->len && l->comp; i++) {
        if (l->comp(data, (*tmp)->userptr) == 0) {
            break;
        }
        tmp = &(*tmp)->next;
    }
    
    return insert_node(l, tmp, data);
}

_public_ int m_list_remove(m_list_t *l, void *data) {
    M_PARAM_ASSERT(m_list_len(l) > 0);
    M_PARAM_ASSERT(data);
    
    list_node **tmp = &l->data;
    for (int i = 0; i < l->len; i++) {
        if ((l->comp && l->comp(data, (*tmp)->userptr) == 0) || (*tmp)->userptr == data) {
            break;
        }
        tmp = &(*tmp)->next;
    }
    return remove_node(l, tmp);
}

_public_ void *m_list_find(m_list_t *l, void *data) {
    M_RET_ASSERT(l, NULL);
    M_RET_ASSERT(data, NULL);
    
    list_node **tmp = &l->data;
    for (int i = 0; i < l->len; i++) {
        if ((l->comp && l->comp(data, (*tmp)->userptr) == 0) || (*tmp)->userptr == data) {
            return (*tmp)->userptr;
        }
        tmp = &(*tmp)->next;
    }
    return NULL;
}

_public_ int m_list_clear(m_list_t *l) {
    M_PARAM_ASSERT(l);
    
    for (m_list_itr_t *itr = m_list_itr_new(l); itr; m_list_itr_next(&itr)) {
        m_list_itr_remove(itr);
    }
    return 0;
}

_public_ int m_list_free(m_list_t **l) {
    M_PARAM_ASSERT(l);
    
    int ret = m_list_clear(*l);
    if (ret == 0) {
        memhook._free(*l);
        *l = NULL;
    }
    return ret;
}

_public_ ssize_t m_list_len(const m_list_t *l) {
    M_PARAM_ASSERT(l);
    
    return l->len;
}
