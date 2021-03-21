#include "poll_priv.h"
#include <sys/epoll.h>

extern void create_priv_fd(ev_src_t *tmp);

typedef struct {
    int fd;
    struct epoll_event *pevents;
} epoll_priv_t;

#define GET_PRIV_DATA()     epoll_priv_t *ep = (epoll_priv_t *)priv->data

int poll_create(poll_priv_t *priv) {
    priv->data = memhook._calloc(1, sizeof(epoll_priv_t));
    M_ALLOC_ASSERT(priv->data);
    GET_PRIV_DATA();
    ep->fd = epoll_create1(EPOLL_CLOEXEC);
    return ep->fd != -1 ? 0 : -1;
}

int poll_set_new_evt(poll_priv_t *priv, ev_src_t *tmp, const enum op_type flag) {
    GET_PRIV_DATA();

    /* Eventually alloc epoll data if needed */
    if (!tmp->ev) {
        if (flag == ADD) {
            tmp->ev = memhook._calloc(1, sizeof(struct epoll_event));
            M_ALLOC_ASSERT(tmp->ev);
        } else {
            /* We need to RM an unregistered ev. Fine. */
            return 0;
        }
    }
    
    int f = flag == ADD ? EPOLL_CTL_ADD : EPOLL_CTL_DEL;
    struct epoll_event *ev = (struct epoll_event *)tmp->ev;
    ev->data.ptr = tmp;
    ev->events = EPOLLIN;
    if (tmp->flags & M_SRC_ONESHOT) {
        ev->events |= EPOLLONESHOT;
    }

    errno = 0;
    if (flag == ADD) {
        create_priv_fd(tmp);
    }
    
    /* 
     * Note that fd_src shares
     * memory space with other fds (inside union)
     */
    const int fd = tmp->fd_src.fd;
    int ret = epoll_ctl(ep->fd, f, fd, ev);
    /* Workaround for STDIN_FILENO: it returns EPERM but it is actually pollable */
    if (ret == -1 && fd == STDIN_FILENO && errno == EPERM) {
        ret = 0;
    }

    /* Eventually free epoll data if needed */
    if (flag == RM) {
        memhook._free(tmp->ev);
        tmp->ev = NULL;
        
        /*
         * Automatically close internally used FDs 
         * for special internal fds 
         */
        if (tmp->type > M_SRC_TYPE_FD) {
            close(fd);
            /* 
             * Reset to -1. Note that fd_src has same
             * memory space as other fds (inside union)
             */
            tmp->fd_src.fd = -1;
        }
    }
    
    return ret;
}

int poll_init(poll_priv_t *priv) {
    GET_PRIV_DATA();
    ep->pevents = memhook._calloc(priv->max_events, sizeof(struct epoll_event));
    M_ALLOC_ASSERT(ep->pevents);
    return 0;
}

int poll_wait(poll_priv_t *priv, const int timeout) {
    GET_PRIV_DATA();
    return epoll_wait(ep->fd, (struct epoll_event *) ep->pevents, priv->max_events, timeout);
}

ev_src_t *poll_recv(poll_priv_t *priv, const int idx) {
    GET_PRIV_DATA();
    if (ep->pevents[idx].events & EPOLLERR) {
        return NULL;
    }
    return (ev_src_t *)ep->pevents[idx].data.ptr;
}

int poll_get_fd(const poll_priv_t *priv) {
    GET_PRIV_DATA();
    return ep->fd;
}

int poll_clear(poll_priv_t *priv) {
    GET_PRIV_DATA();
    memhook._free(ep->pevents);
    ep->pevents = NULL;
    return 0;
}

int poll_destroy(poll_priv_t *priv) {
    GET_PRIV_DATA();
    poll_clear(priv);
    close(ep->fd);
    return 0;
}
