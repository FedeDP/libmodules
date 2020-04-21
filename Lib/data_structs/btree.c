#include "poll_priv.h"
#include "btree.h"

typedef struct _elem {
    void *userptr;
    struct _elem *right;
    struct _elem *left;
} btree_node;

struct _btree {
    size_t len;
    btree_node *root;
    m_btree_cmp comp;
    m_btree_dtor dtor;
};

struct _btree_itr {
    bool removed;
    btree_node **elem;
    m_btree_t *l;
};


static inline int insert_node(m_btree_t *l, btree_node **elem, void *data);
static inline int remove_node(m_btree_t *l, btree_node **elem);
static void destroy_tree(m_btree_t *l, btree_node *root);
static int ptrcmp(void *userdata, void *node_data);
static inline int traverse_preorder(btree_node *node, m_btree_cb cb, void *userptr);
static inline int traverse_postorder(btree_node *node, m_btree_cb cb, void *userptr);
static inline int traverse_inorder(btree_node *node, m_btree_cb cb, void *userptr);
static inline btree_node **btree_find(m_btree_t *l, void *data);

static inline int insert_node(m_btree_t *l, btree_node **elem, void *data) {
    btree_node *node = memhook._calloc(1, sizeof(btree_node));
    MOD_ALLOC_ASSERT(node);
    node->userptr = data;
    *elem = node;
    l->len++;
    return 0;
}

static inline int remove_node(m_btree_t *l, btree_node **elem) {
    btree_node *root = *elem;
    if (root) {
        /* Nodes with only one child or no child */
        if (!root->left || !root->right) {
            btree_node *new_node = NULL;
            if (!root->left) {
                new_node = root->right;
            } else {
                new_node = root->left;
            }
            *elem = new_node;
            memhook._free(root);
        } else {
            /*
             * Node with two children: get inorder successor
             * (smallest in the right subtree)
             */
            btree_node **tmp = &root->right;
            while ((*tmp)->left) {
                tmp = &(*tmp)->left;
            }
            root->userptr = (*tmp)->userptr; // switch userdata
            return remove_node(l, tmp); // remove useless left-most node in the right subtree
        }
        l->len--;
        return 0;
    }
    return -ENOENT;
}

static void destroy_tree(m_btree_t *l, btree_node *root) {
    if (root) {
        destroy_tree(l, root->left);
        destroy_tree(l, root->right);
        memhook._free(root);
        l->len--;
    }
}

static int ptrcmp(void *userdata, void *node_data) {
    return (userdata - node_data);
}

static inline int traverse_preorder(btree_node *node, m_btree_cb cb, void *userptr) {
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

static inline int traverse_postorder(btree_node *node, m_btree_cb cb, void *userptr) {
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

static inline int traverse_inorder(btree_node *node, m_btree_cb cb, void *userptr) {
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

static inline btree_node **btree_find(m_btree_t *l, void *data) {
    btree_node **tmp = &l->root;
    while (*tmp) {
        int ret = l->comp(data, (*tmp)->userptr);
        if (ret > 0) {
            tmp = &(*tmp)->right;
        } else if (ret < 0) {
            tmp = &(*tmp)->left;
        } else {
            break;
        }
    }
    return tmp;
}

/** Public API **/

m_btree_t *m_btree_new(const m_btree_cmp comp, const m_btree_dtor fn) {
    m_btree_t *l = memhook._calloc(1, sizeof(m_btree_t));
    if (l) {
        l->dtor = fn;
        l->comp = comp ? comp : ptrcmp;
    }
    return l;
}

int m_btree_insert(m_btree_t *l, void *data) {
    MOD_PARAM_ASSERT(l);
    MOD_PARAM_ASSERT(data);
    
    btree_node **new_node = btree_find(l, data);
    if (*new_node) {
        return -EEXIST;
    }
    return insert_node(l, new_node, data);
}

int m_btree_remove(m_btree_t *l, void *data) {
    MOD_PARAM_ASSERT(m_btree_length(l) > 0);
    MOD_PARAM_ASSERT(data);
    
    btree_node **new_node = btree_find(l, data);
    if (new_node) {
        return remove_node(l, new_node);
    }
    return -ENOENT;
}

void *m_btree_find(m_btree_t *l, void *data) {
    MOD_RET_ASSERT(l, NULL);
    MOD_RET_ASSERT(data, NULL);
    
    btree_node **new_node = btree_find(l, data);
    if (*new_node) {
        return (*new_node)->userptr;
    }
    return NULL;
}

int m_btree_traverse(m_btree_t *l, m_btree_order type, m_btree_cb cb, void *userptr) {
    int ret;
    switch (type) {
    case M_BTREE_PRE:
        ret = traverse_preorder(l->root, cb, userptr);
        break;
    case M_BTREE_POST:
        ret = traverse_postorder(l->root, cb, userptr);
        break;
    case M_BTREE_IN:
        ret = traverse_inorder(l->root, cb, userptr);
        break;
    default:
        ret = -EINVAL;
        break;
    }
    return ret >= 0 ? 0 : ret;
}

m_btree_itr_t *m_btree_itr_new(const m_btree_t *l) {
    MOD_RET_ASSERT(m_btree_length(l) > 0, NULL);

    m_btree_itr_t *itr = memhook._calloc(1, sizeof(m_btree_itr_t));
    if (itr) {
        itr->elem = (btree_node **)&(l->root);
        itr->l = (m_btree_t *)l;
    }
    return itr;
}

m_btree_itr_t *m_btree_itr_next(m_btree_itr_t *itr) {
    MOD_RET_ASSERT(itr, NULL);

    if (*(itr->elem) && !itr->removed) {
        btree_node **left = &(*(itr->elem))->left;
        btree_node **right = &(*(itr->elem))->right;
        if (left) {
            itr->elem = left;
        } else {
            itr->elem = right;
        }
    }
    itr->removed = false;
    if (!*(itr->elem)) {
        memhook._free(itr);
        itr = NULL;
    }
    return itr;
}

void *m_btree_itr_get_data(const m_btree_itr_t *itr) {
    MOD_RET_ASSERT(itr, NULL);
    MOD_RET_ASSERT(*itr->elem, NULL);

    return (*itr->elem)->userptr;
}

int m_btree_itr_remove(m_btree_itr_t *itr) {
    MOD_PARAM_ASSERT(itr);
    MOD_RET_ASSERT(*itr->elem, -EINVAL);

    if (!itr->removed) {
        itr->removed = true;
        return remove_node(itr->l, itr->elem);
    }
    return -EPERM;
}

int m_btree_clear(m_btree_t *l) {
    MOD_PARAM_ASSERT(m_btree_length(l) > 0);
    
    destroy_tree(l, l->root);
    l->root = NULL;
    return 0;
}

int m_btree_free(m_btree_t **l) {
    MOD_PARAM_ASSERT(l);
    
    m_btree_clear(*l);
    memhook._free(*l);
    *l = NULL;
    return 0;
}

ssize_t m_btree_length(const m_btree_t *l) {
    MOD_PARAM_ASSERT(l);
    
    return l->len;
}

