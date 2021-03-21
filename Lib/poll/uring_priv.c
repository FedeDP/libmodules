#include "poll_priv.h"
#include <liburing.h>
#include <sys/poll.h>

#warning "Uring support is unstable."

/*
 * Doc: https://kernel.dk/io_uring.pdf
 * 
 * Naming: 
 * SQE -> Submission Queue Event
 * CQE -> Completion Queue Event
 */

extern void create_priv_fd(ev_src_t *tmp);

typedef struct {
    struct io_uring ring;
    struct io_uring_cqe *cqe;
} uring_priv_t;

#define GET_PRIV_DATA()     uring_priv_t *up = (uring_priv_t *)priv->data

int poll_create(poll_priv_t *priv) {
    priv->data = memhook._calloc(1, sizeof(uring_priv_t));
    M_ALLOC_ASSERT(priv->data);

    GET_PRIV_DATA();
    /*
     * Initialize queue with M_CTX_DEFAULT_EVENTS because uring must be UP
     * before registering event sources; but we do not yet know
     * priv->max_events.
     * Anyway, it is always set to M_CTX_DEFAULT_EVENTS right now.
     */
    return io_uring_queue_init(M_CTX_DEFAULT_EVENTS, &up->ring, IORING_SETUP_IOPOLL);
}

int poll_set_new_evt(poll_priv_t *priv, ev_src_t *tmp, const enum op_type flag) {
    GET_PRIV_DATA();
    
    int ret = 0;
    /* Eventually request uring sqe if needed */
    if (!tmp->ev) {
        if (flag == ADD) {
            tmp->ev = io_uring_get_sqe(&up->ring);
            M_ALLOC_ASSERT(tmp->ev);
        } else {
            /* We need to RM an unregistered ev. Fine. */
            return 0;
        }
    }
        
    struct io_uring_sqe *sqe = (struct io_uring_sqe *)tmp->ev;
    if (flag == ADD && tmp->fd_src.fd == -1) {
        create_priv_fd(tmp);
    }
        
    /*
     * Note that fd_src shares
     * memory space with other fds (inside union)
     */
    const int fd = tmp->fd_src.fd;
    if (fd != -1) {
        if (flag == ADD) {
            io_uring_prep_poll_add(sqe, fd, POLLIN);
            /* properly set userdata */
            io_uring_sqe_set_data(sqe, tmp);
        } else {
            io_uring_prep_poll_remove(sqe, tmp);
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
    } else {
        ret = -1;
    }
    return ret;
}

int poll_init(poll_priv_t *priv) {
    return 0;
}

int poll_wait(poll_priv_t *priv, const int timeout) {
    GET_PRIV_DATA();

    io_uring_submit(&up->ring);
    struct __kernel_timespec t = {0};
    t.tv_sec = timeout;
    int ret = io_uring_wait_cqe_timeout(&up->ring, &up->cqe, timeout >= 0 ? &t : NULL);
    if (ret == 0) {
        return 1;
    }
    return ret; // errno error code
}

ev_src_t *poll_recv(poll_priv_t *priv, const int idx) {
    GET_PRIV_DATA();
    ev_src_t *udata = (ev_src_t *)io_uring_cqe_get_data(up->cqe);
    io_uring_cqe_seen(&up->ring, up->cqe);

    /* udata != NULL only if fd is still registered */
    if (udata) {
        /* We need to re-arm it: IORING_OP_POLL_ADD interface works in oneshot mode */
        udata->ev = NULL;
        poll_set_new_evt(priv, udata, ADD);
    }
    return udata;
}

int poll_get_fd(const poll_priv_t *priv) {
    GET_PRIV_DATA();
    return up->ring.ring_fd;
}

int poll_clear(poll_priv_t *priv) {
    return 0;
}

int poll_destroy(poll_priv_t *priv) {
    GET_PRIV_DATA();
    io_uring_queue_exit(&up->ring);
    return 0;
}