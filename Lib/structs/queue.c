#include "priv.h"

typedef struct _elem {
    void *userptr;
    struct _elem *prev;
} queue_elem;

struct _queue {
    size_t len;
    m_queue_dtor dtor;
    queue_elem *head;
    queue_elem *tail;
};

struct _queue_itr {
    m_queue_t *q;
    queue_elem **elem;
    bool removed;
};

/** Public API **/

_public_ m_queue_t *m_queue_new(m_queue_dtor fn) {
    m_queue_t *q = memhook._calloc(1, sizeof(m_queue_t));
    if (q) {
        q->dtor = fn;
    }
    return q;
}

_public_ m_queue_itr_t *m_queue_itr_new(const m_queue_t *q) {
    M_RET_ASSERT(m_queue_len(q) > 0, NULL);
    
    m_queue_itr_t *itr = memhook._calloc(1, sizeof(m_queue_itr_t));
    if (itr) {
        itr->elem = (queue_elem **)&q->head;
        itr->q = (m_queue_t *)q;
    }
    return itr;
}

_public_ int m_queue_itr_next(m_queue_itr_t **itr) {
    M_PARAM_ASSERT(itr && *itr);
    
    m_queue_itr_t *i = *itr;
    if (!i->removed) {
        i->elem = &(*i->elem)->prev;
    } else {
        i->removed = false;
    }
    if (!*i->elem) {
        memhook._free(*itr);
        *itr = NULL;
    }
    return 0;
}

_public_ int m_queue_itr_remove(m_queue_itr_t *itr) {
    M_PARAM_ASSERT(itr && !itr->removed);
    
    queue_elem *tmp = *itr->elem;
    if (tmp) {
        *itr->elem = (*itr->elem)->prev;
        if (itr->q->dtor) {
            itr->q->dtor(tmp->userptr);
        }
        memhook._free(tmp);
        itr->q->len--;
        itr->removed = true;
        return 0;
    }
    return -ENOENT;
}

_public_ void *m_queue_itr_get_data(const m_queue_itr_t *itr) {
    M_RET_ASSERT(itr && !itr->removed, NULL);
    
    return (*itr->elem)->userptr;
}

_public_ int m_queue_itr_set_data(const m_queue_itr_t *itr, void *value) {
    M_PARAM_ASSERT(itr && !itr->removed);
    M_PARAM_ASSERT(value);
    
    (*itr->elem)->userptr = value;
    return 0;
}

_public_ int m_queue_iterate(const m_queue_t *q, m_queue_cb fn, void *userptr) {
    M_PARAM_ASSERT(fn);
    M_PARAM_ASSERT(m_queue_len(q) > 0);
    
    queue_elem *elem = q->head;
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
        elem = elem->prev;
    }
    return 0;
}

_public_ int m_queue_enqueue(m_queue_t *q, void *data) {
    M_PARAM_ASSERT(q);
    M_PARAM_ASSERT(data);
    
    queue_elem *elem = memhook._calloc(1, sizeof(queue_elem));
    M_ALLOC_ASSERT(elem);
    q->len++;
    elem->userptr = data;
    if (q->tail) {
        q->tail->prev = elem;
    }
    q->tail = elem;
    if (!q->head) {
        q->head = q->tail;
    }
    return 0;
}

_public_ void *m_queue_dequeue(m_queue_t *q) {
    M_RET_ASSERT(m_queue_len(q) > 0, NULL);
    
    queue_elem *elem = q->head;
    if (q->tail == q->head) {
        q->tail = NULL;
    }
    q->head = q->head->prev;
    
    void *data = elem->userptr;
    memhook._free(elem);
    q->len--;
    return data;
}

_public_ void *m_queue_peek(const m_queue_t *q) {
    M_RET_ASSERT(m_queue_len(q) > 0, NULL);
    
    return q->head->userptr;
}

_public_ int m_queue_remove(m_queue_t *q) {
    void *data = m_queue_dequeue(q);
    if (data) {
        if (q->dtor) {
            q->dtor(data);
        }
        return 0;
    }
    return -EINVAL;
}

_public_ int m_queue_clear(m_queue_t *q) {
    M_PARAM_ASSERT(m_queue_len(q) > 0);
    
    while (q->len > 0) {
        m_queue_remove(q);
    }
    return 0;
}

_public_ int m_queue_free(m_queue_t **q) {
    M_PARAM_ASSERT(q);
    
    m_queue_clear(*q);
    memhook._free(*q);
    *q = NULL;
    return 0;
}

_public_ ssize_t m_queue_len(const m_queue_t *q) {
    M_PARAM_ASSERT(q);
    
    return q->len;
}

