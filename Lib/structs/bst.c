#include "priv.h"

typedef struct _elem {
    void *userptr;
    struct _elem *parent;
    struct _elem *right;
    struct _elem *left;
} bst_node;

struct _bst {
    size_t len;
    bst_node *root;
    m_bst_cmp comp;
    m_bst_dtor dtor;
};

struct _bst_itr {
    bool removed;
    bst_node **curr;
    bst_node *prev;
    m_bst_t *l;
};

static inline int insert_node(m_bst_t *l, bst_node **elem, bst_node *parent, void *data);
static inline bst_node **find_min_subtree(bst_node **node);
static inline int remove_node(m_bst_t *l, bst_node **elem);
static int ptrcmp(void *userdata, void *node_data);
static inline int traverse_preorder(bst_node *node, m_bst_cb cb, void *userptr);
static inline int traverse_postorder(bst_node *node, m_bst_cb cb, void *userptr);
static inline int traverse_inorder(bst_node *node, m_bst_cb cb, void *userptr);
static inline bst_node **bst_find(m_bst_t *l, void *data, bst_node **parent);
static inline bst_node **bst_next(bst_node **node);

static inline int insert_node(m_bst_t *l, bst_node **elem, bst_node *parent, void *data) {
    bst_node *node = memhook._calloc(1, sizeof(bst_node));
    M_ALLOC_ASSERT(node);
    node->userptr = data;
    node->parent = parent;
    *elem = node;
    l->len++;
    return 0;
}

static inline bst_node **find_min_subtree(bst_node **node) {
    while ((*node)->left) {
        node = &(*node)->left;
    }
    return node;
}

static inline int remove_node(m_bst_t *l, bst_node **elem) {
    bst_node *node = *elem;
    if (node) {
        /* Nodes with at most one child */
        if (!node->left || !node->right) {
            if (node->left) {
                *elem = node->left;
            } else {
                *elem = node->right;
            }
            if (*elem) {
                /* Update node parent */
                (*elem)->parent = node->parent;
            }
            if (l->dtor) {
                l->dtor(node->userptr);
            }
            memhook._free(node);
            l->len--;
            return 0;
        }
        /*
         * Nodes with two children: get inorder successor
         * (smallest in the right subtree)
         */
        bst_node **tmp = find_min_subtree(&node->right);
        node->userptr = (*tmp)->userptr; // switch userdata
        return remove_node(l, tmp); // remove useless left-most node in the right subtree
    }
    return -ENOENT;
}

static int ptrcmp(void *userdata, void *node_data) {
    return (userdata - node_data);
}

static inline int traverse_preorder(bst_node *node, m_bst_cb cb, void *userptr) {
    if (node) {
        int ret = cb(userptr, node->userptr);
        if (ret == 0) {
            ret = traverse_preorder(node->left, cb, userptr);
            if (ret == 0) {
                ret = traverse_preorder(node->right, cb, userptr);
            }
        }
        return ret;
    }
    return 0;
}

static inline int traverse_postorder(bst_node *node, m_bst_cb cb, void *userptr) {
    if (node) {
        int ret = traverse_postorder(node->left, cb, userptr);
        if (ret == 0) {
            ret = traverse_postorder(node->right, cb, userptr);
            if (ret == 0) {
                ret = cb(userptr, node->userptr);
            }
        }
        return ret;
    }
    return 0;
}

static inline int traverse_inorder(bst_node *node, m_bst_cb cb, void *userptr) {
    if (node) {
        int ret = traverse_inorder(node->left, cb, userptr);
        if (ret == 0) {
            ret = cb(userptr, node->userptr);
            if (ret == 0) {
                ret = traverse_inorder(node->right, cb, userptr);
            }
        }
        return ret;
    }
    return 0;
}

static inline bst_node **bst_find(m_bst_t *l, void *data, bst_node **parent) {
    bst_node **tmp = &l->root;
    while (*tmp) {
        const int ret = l->comp(data, (*tmp)->userptr);
        if (ret) {
            if (parent) {
                *parent = *tmp;
            }
            if (ret > 0) {
                tmp = &(*tmp)->right;
            } else {
                tmp = &(*tmp)->left;
            }
        } else {
            break;
        }
    }
    return tmp;
}

/* Thanks to: https://www.cs.odu.edu/~zeil/cs361/latest/Public/treetraversal/index.html#working-with-parents */
static inline bst_node **bst_next(bst_node **node) {
    if (*node) {
        if ((*node)->right) {
            node = find_min_subtree(&(*node)->right);
        } else {
            /*
             * We already processed the left subtree, and
             * there is no right subtree. 
             * Move up the tree, looking for a parent for which curr is a right child,
             * stopping if the parent becomes NULL. 
             * A non-NULL parent is the successor. 
             * If parent is NULL, the original node was the last node inorder, 
             * and its successor is the end of the list
             */
            bst_node **parent = &(*node)->parent;
            while (*parent && *node == (*parent)->right) {
                node = parent;
                parent = &(*node)->parent;
            }
            /*
             * If we were previously at the right-most node in
             * the tree, the iterator specifies the end of the list
             */
            node = parent;
        }
    }
    return node;
}

/** Public API **/

_public_ m_bst_t *m_bst_new(m_bst_cmp comp, m_bst_dtor fn) {
    m_bst_t *l = memhook._calloc(1, sizeof(m_bst_t));
    if (l) {
        l->dtor = fn;
        l->comp = comp ? comp : ptrcmp;
    }
    return l;
}

_public_ int m_bst_insert(m_bst_t *l, void *data) {
    M_PARAM_ASSERT(l);
    M_PARAM_ASSERT(data);
    
    bst_node *parent = NULL;
    bst_node **node = bst_find(l, data, &parent);
    if (*node) {
        return -EEXIST;
    }
    return insert_node(l, node, parent, data);
}

_public_ int m_bst_remove(m_bst_t *l, void *data) {
    M_PARAM_ASSERT(m_bst_len(l) > 0);
    M_PARAM_ASSERT(data);
    
    bst_node **node = bst_find(l, data, NULL);
    if (node) {
        return remove_node(l, node);
    }
    return -ENOENT;
}

_public_ void *m_bst_find(m_bst_t *l, void *data) {
    M_RET_ASSERT(l, NULL);
    M_RET_ASSERT(data, NULL);
    
    bst_node **node = bst_find(l, data, NULL);
    if (*node) {
        return (*node)->userptr;
    }
    return NULL;
}

_public_ int m_bst_iterate(m_bst_t *l, m_bst_cb cb, void *userptr) {
    return m_bst_traverse(l, M_BST_PRE, cb, userptr);
}

_public_ int m_bst_traverse(m_bst_t *l, m_bst_order type, m_bst_cb cb, void *userptr) {
    M_PARAM_ASSERT(l);
    M_PARAM_ASSERT(cb);
    
    int ret;
    switch (type) {
    case M_BST_PRE:
        ret = traverse_preorder(l->root, cb, userptr);
        break;
    case M_BST_POST:
        ret = traverse_postorder(l->root, cb, userptr);
        break;
    case M_BST_IN:
        ret = traverse_inorder(l->root, cb, userptr);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret >= 0 ? 0 : ret;
}

_public_ m_bst_itr_t *m_bst_itr_new(const m_bst_t *l) {
    M_RET_ASSERT(m_bst_len(l) > 0, NULL);

    m_bst_itr_t *itr = memhook._calloc(1, sizeof(m_bst_itr_t));
    if (itr) {
        itr->curr = find_min_subtree((bst_node **)&l->root);
        itr->l = (m_bst_t *)l;
    }
    return itr;
}

_public_ int m_bst_itr_next(m_bst_itr_t **itr) {
    M_PARAM_ASSERT(itr && *itr);

    m_bst_itr_t *i = *itr;
    if (!i->removed) {
        i->prev = *i->curr;
        i->curr = bst_next(i->curr);
    } else if (i->prev) {
        /* If we have a previous element, find new next of that element */
        i->curr = bst_next(&i->prev);
    } else if (i->l->root) {
        /* 
         * If we haven't got a previous elem, it means we have freed
         * first iterator (or we are freeing all iterators)
         * Next will be new root's min_subtree 
         */
        i->curr = find_min_subtree((bst_node **)&i->l->root);
    } else {
        /* Everything was removed. End iteration. */
        i->curr = &i->l->root;
    }
    i->removed = false;
    if (!*i->curr) {
        memhook._free(*itr);
        *itr = NULL;
    }
    return 0;
}

_public_ int m_bst_itr_remove(m_bst_itr_t *itr) {
    M_PARAM_ASSERT(itr && !itr->removed);
    M_PARAM_ASSERT(*(itr->curr));
    
    /*
     * remove_node() does expect address of pointer to
     * node as seen by its parent (ie: &parent->{left,right}).
     * While iterating, our node can be reached through 
     * its child 'parent' pointer, (ie: &child->parent);
     * This happens eg: while climbing up left subtree.
     * 
     * Normalize to expected remove_node() pointer.
     */
    bst_node **node = &(*itr->curr)->parent;
    if (*node) {
        if (*itr->curr == (*node)->right) {
            node = &(*node)->right;
        } else {
            node = &(*node)->left;
        }
    } else {
        node = &(itr->l->root);
    }
    int ret = remove_node(itr->l, node);
    itr->removed = ret == 0;
    return ret;
}

_public_ void *m_bst_itr_get_data(const m_bst_itr_t *itr) {
    M_RET_ASSERT(itr && !itr->removed, NULL);
    M_RET_ASSERT(*(itr->curr), NULL);

    return (*itr->curr)->userptr;
}

_public_ int m_bst_clear(m_bst_t *l) {
    M_PARAM_ASSERT(m_bst_len(l) > 0);
    
    for (m_bst_itr_t *itr = m_bst_itr_new(l); itr; m_bst_itr_next(&itr)) {
        m_bst_itr_remove(itr);
    }
    l->root = NULL;
    return 0;
}

_public_ int m_bst_free(m_bst_t **l) {
    M_PARAM_ASSERT(l);
    
    m_bst_clear(*l);
    memhook._free(*l);
    *l = NULL;
    return 0;
}

_public_ ssize_t m_bst_len(const m_bst_t *l) {
    M_PARAM_ASSERT(l);
    
    return l->len;
}

