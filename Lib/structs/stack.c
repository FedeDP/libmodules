#include "priv.h"

typedef struct _elem {
    void *userptr;
    struct _elem *prev;
} stack_elem;

struct _stack {
    size_t len;
    m_stack_dtor dtor;
    stack_elem *data;
};

struct _stack_itr {
    m_stack_t *s;
    stack_elem **elem;
    bool removed;
};

/** Public API **/

_public_ m_stack_t *m_stack_new(m_stack_dtor fn) {
    m_stack_t *s = memhook._calloc(1, sizeof(m_stack_t));
    if (s) {
        s->dtor = fn;
    }
    return s;
}

_public_ m_stack_itr_t *m_stack_itr_new(const m_stack_t *s) {
    M_RET_ASSERT(m_stack_len(s) > 0, NULL);
    
    m_stack_itr_t *itr = memhook._calloc(1, sizeof(m_stack_itr_t));
    if (itr) {
        itr->s = (m_stack_t *)s;
        itr->elem = (stack_elem **)&s->data;
    }
    return itr;
}

_public_ int m_stack_itr_next(m_stack_itr_t **itr) {
    M_PARAM_ASSERT(itr && *itr);
    
    m_stack_itr_t *i = *itr;
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

_public_ int m_stack_itr_remove(m_stack_itr_t *itr) {
    M_PARAM_ASSERT(itr && !itr->removed);
    
    stack_elem *tmp = *itr->elem;
    if (tmp) {
        *itr->elem = (*itr->elem)->prev;
        if (itr->s->dtor) {
            itr->s->dtor(tmp->userptr);
        }
        memhook._free(tmp);
        itr->s->len--;
        itr->removed = true;
        return 0;
    }
    return -ENOENT;
}

_public_ void *m_stack_itr_get_data(const m_stack_itr_t *itr) {
    M_RET_ASSERT(itr && !itr->removed, NULL);
    
    return (*itr->elem)->userptr;
}

_public_ int m_stack_itr_set_data(const m_stack_itr_t *itr, void *value) {
    M_PARAM_ASSERT(itr && !itr->removed);
    M_PARAM_ASSERT(value);
    
    (*itr->elem)->userptr = value;
    return 0;
}

_public_ int m_stack_iterate(const m_stack_t *s, m_stack_cb fn, void *userptr) {
    M_PARAM_ASSERT(fn);
    M_PARAM_ASSERT(m_stack_len(s) > 0);
    
    stack_elem *elem = s->data;
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

_public_ int m_stack_push(m_stack_t *s, void *data) {
    M_PARAM_ASSERT(s);
    M_PARAM_ASSERT(data);
    
    stack_elem *elem = memhook._malloc(sizeof(stack_elem));
    M_ALLOC_ASSERT(elem);
    s->len++;
    elem->userptr = data;
    elem->prev = s->data;
    s->data = elem;
    return 0;
}

_public_ void *m_stack_pop(m_stack_t *s) {
    M_RET_ASSERT(m_stack_len(s) > 0, NULL);

    stack_elem *elem = s->data;
    s->data = s->data->prev;
    void *data = elem->userptr;
    memhook._free(elem);
    s->len--;
    return data;
}

_public_ void *m_stack_peek(const m_stack_t *s) {
    M_RET_ASSERT(m_stack_len(s) > 0, NULL);
    
    return s->data->userptr; // return most recent element data
}

_public_ int m_stack_clear(m_stack_t *s) {
    M_PARAM_ASSERT(s);
    
    while (s->len > 0) {
        m_stack_remove(s);
    }
    return 0;
}

_public_ int m_stack_remove(m_stack_t *s) {
    void *data = m_stack_pop(s);
    if (data) {
        if (s->dtor) {
            s->dtor(data);
        }
        return 0;
    }
    return -EINVAL;
}

_public_ int m_stack_free(m_stack_t **s) {
    M_PARAM_ASSERT(s);
    
    int ret = m_stack_clear(*s);
    if (ret == 0) {
        memhook._free(*s);
        *s = NULL;
    }
    return ret;
}

_public_ ssize_t m_stack_len(const m_stack_t *s) {
    M_PARAM_ASSERT(s);
    
    return s->len;
}
