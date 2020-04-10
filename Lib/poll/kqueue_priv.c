#include "poll_priv.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>

typedef struct {
    int fd;
    struct kevent *pevents;
} kqueue_priv_t;

#define GET_PRIV_DATA()     kqueue_priv_t *kp = (kqueue_priv_t *)priv->data

mod_ret poll_create(poll_priv_t *priv) {
    priv->data = memhook._calloc(1, sizeof(kqueue_priv_t));
    MOD_ALLOC_ASSERT(priv->data);
    GET_PRIV_DATA();
    kp->fd = kqueue();
    fcntl(kp->fd, F_SETFD, FD_CLOEXEC);
    return MOD_OK;
}

int poll_set_new_evt(poll_priv_t *priv, ev_src_t *tmp, const enum op_type flag) {
    static int timer_ids = 1;
    GET_PRIV_DATA();
    
    /* Eventually alloc kqueue data if needed */
    if (!tmp->ev) {
        if (flag == ADD) {
            tmp->ev = memhook._calloc(1, sizeof(struct kevent));
            MOD_ALLOC_ASSERT(tmp->ev);
        } else {
            /* We need to RM an unregistered ev. Fine. */
            return MOD_OK;
        }
    }

    int f = flag == ADD ? EV_ADD : EV_DELETE;
    if (tmp->flags & SRC_ONESHOT) {
        f |= EV_ONESHOT;
    }
    struct kevent *_ev = (struct kevent *)tmp->ev;
    switch (tmp->type) {
    case TYPE_PS: // TYPE_PS is used for pubsub_fd[0] in init_pubsub_fd()
    case TYPE_FD:
        EV_SET(_ev, tmp->fd_src.fd, EVFILT_READ, f, 0, 0, tmp);
        break;
    case TYPE_TMR: {
#if defined NOTE_ABSOLUTE // Apple (https://www.unix.com/man-page/osx/2/kqueue/)
        const int abs_fl = tmp->flags & SRC_TMR_ABSOLUTE ? NOTE_ABSOLUTE : 0;
#elif defined NOTE_ABSTIME // BSD (https://www.freebsd.org/cgi/man.cgi?query=kqueue&sektion=2)
        const int abs_fl = tmp->flags & SRC_TMR_ABSOLUTE ? NOTE_ABSTIME : 0;
#else
        const int  abs_fl = 0; // unsupported...
#endif
        EV_SET(_ev, timer_ids++, EVFILT_TIMER, f, abs_fl, tmp->tmr_src.its.ms, tmp);
        break;
    }
    case TYPE_SGN:
        EV_SET(_ev, tmp->sgn_src.sgs.signo, EVFILT_SIGNAL, f, 0, 0, tmp);
        break;
    case TYPE_PT: 
        tmp->pt_src.f.fd = open(tmp->pt_src.pt.path, O_RDONLY);
        if (tmp->pt_src.f.fd != -1) {
            EV_SET(_ev, tmp->pt_src.f.fd, EVFILT_VNODE, f, tmp->pt_src.pt.events, 0, tmp);
        } else {
            return -1;
        }
        break;
    case TYPE_PID:
        EV_SET(_ev, tmp->pid_src.pid.pid, EVFILT_PROC, f, tmp->pid_src.pid.events, 0, tmp);
        break;
    default: 
        break;
    }

    int ret = kevent(kp->fd, _ev, 1, NULL, 0, NULL);
    /* Workaround for STDIN_FILENO: it is actually pollable */
    if (tmp->type == TYPE_FD && tmp->fd_src.fd == STDIN_FILENO) {
        ret = 0;
    }
    
    /* Eventually free kqueue data if needed */
    if (flag == RM) {
        memhook._free(tmp->ev);
        tmp->ev = NULL;
        
        if (tmp->type == TYPE_PT) {
            close(tmp->pt_src.f.fd); // automatically close internally used FDs
            tmp->pt_src.f.fd = -1;
        }
    }
    
    return ret;
}

mod_ret poll_init(poll_priv_t *priv) {
    GET_PRIV_DATA();
    kp->pevents = memhook._calloc(priv->max_events, sizeof(struct kevent));
    MOD_ALLOC_ASSERT(kp->pevents);
    return MOD_OK;
}

int poll_wait(poll_priv_t *priv, const int timeout) {
    GET_PRIV_DATA();
    struct timespec t = {0};
    t.tv_sec = timeout;
    return kevent(kp->fd, NULL, 0, kp->pevents, priv->max_events, timeout >= 0 ? &t : NULL);
}

ev_src_t *poll_recv(poll_priv_t *priv, const int idx) {
    GET_PRIV_DATA();
    if (kp->pevents[idx].flags & EV_ERROR) {
        return NULL;
    }
    return (ev_src_t *)kp->pevents[idx].udata;
}

mod_ret poll_consume_sgn(poll_priv_t *priv, const int idx, ev_src_t *src, sgn_msg_t *sgn_msg) {
    return MOD_OK;
}

mod_ret poll_consume_tmr(poll_priv_t *priv, const int idx, ev_src_t *src, tmr_msg_t *tm_msg) {
    return MOD_OK;
}

mod_ret poll_consume_pt(poll_priv_t *priv, const int idx, ev_src_t *src, pt_msg_t *pt_msg) {
    GET_PRIV_DATA();
    pt_msg->events = kp->pevents[idx].fflags;
    return MOD_OK;
}

mod_ret poll_consume_pid(poll_priv_t *priv, const int idx, ev_src_t *src, pid_msg_t *pid_msg) {
    GET_PRIV_DATA();
    pid_msg->events = kp->pevents[idx].fflags;
    return MOD_OK;
}

int poll_get_fd(poll_priv_t *priv) {
    GET_PRIV_DATA();
    return kp->fd;
}

mod_ret poll_clear(poll_priv_t *priv) {
    GET_PRIV_DATA();
    memhook._free(kp->pevents);
    kp->pevents = NULL;
    return MOD_OK;
}

mod_ret poll_destroy(poll_priv_t *priv) {
    GET_PRIV_DATA();
    poll_clear(priv);
    close(kp->fd);
    memhook._free(kp);
    return MOD_OK;
}