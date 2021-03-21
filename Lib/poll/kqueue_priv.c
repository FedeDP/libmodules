#include "poll_priv.h"
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <math.h>

typedef struct {
    int fd;
    struct kevent *pevents;
} kqueue_priv_t;

#define GET_PRIV_DATA()     kqueue_priv_t *kp = (kqueue_priv_t *)priv->data

int poll_create(poll_priv_t *priv) {
    priv->data = memhook._calloc(1, sizeof(kqueue_priv_t));
    M_ALLOC_ASSERT(priv->data);
    GET_PRIV_DATA();
    kp->fd = kqueue();
    fcntl(kp->fd, F_SETFD, FD_CLOEXEC);
    return 0;
}

int poll_set_new_evt(poll_priv_t *priv, ev_src_t *tmp, const enum op_type flag) {
    static int timer_ids = 1;
    GET_PRIV_DATA();
    
    /* Eventually alloc kqueue data if needed */
    if (!tmp->ev) {
        if (flag == ADD) {
            tmp->ev = memhook._calloc(1, sizeof(struct kevent));
            M_ALLOC_ASSERT(tmp->ev);
        } else {
            /* We need to RM an unregistered ev. Fine. */
            return 0;
        }
    }

    int f = flag == ADD ? EV_ADD : EV_DELETE;
    if (tmp->flags & M_SRC_ONESHOT) {
        f |= EV_ONESHOT;
    }
    struct kevent *_ev = (struct kevent *)tmp->ev;
    switch (tmp->type) {
    case M_SRC_TYPE_PS: // M_SRC_TYPE_PS is used for pubsub_fd[0] in init_pubsub_fd()
    case M_SRC_TYPE_FD:
        EV_SET(_ev, tmp->fd_src.fd, EVFILT_READ, f, 0, 0, tmp);
        break;
    case M_SRC_TYPE_TMR: {
#if defined NOTE_ABSOLUTE // Apple (https://www.unix.com/man-page/osx/2/kqueue/)
        const int abs_fl = tmp->flags & M_SRC_TMR_ABSOLUTE ? NOTE_ABSOLUTE : 0;
#elif defined NOTE_ABSTIME // BSD (https://www.freebsd.org/cgi/man.cgi?query=kqueue&sektion=2)
        const int abs_fl = tmp->flags & M_SRC_TMR_ABSOLUTE ? NOTE_ABSTIME : 0;
#else
        const int  abs_fl = 0; // unsupported...
#endif
        EV_SET(_ev, timer_ids++, EVFILT_TIMER, f, abs_fl, tmp->tmr_src.its.ms, tmp);
        break;
    }
    case M_SRC_TYPE_SGN:
        EV_SET(_ev, tmp->sgn_src.sgs.signo, EVFILT_SIGNAL, f, 0, 0, tmp);
        break;
    case M_SRC_TYPE_PATH: 
        tmp->path_src.f.fd = open(tmp->path_src.pt.path, O_RDONLY);
        if (tmp->path_src.f.fd != -1) {
            EV_SET(_ev, tmp->path_src.f.fd, EVFILT_VNODE, f, tmp->path_src.pt.events, 0, tmp);
        } else {
            return -EBADF;
        }
        break;
    case M_SRC_TYPE_PID:
        EV_SET(_ev, tmp->pid_src.pid.pid, EVFILT_PROC, f, tmp->pid_src.pid.events, 0, tmp);
        break;
    case M_SRC_TYPE_TASK:
    case M_SRC_TYPE_THRESH:
        EV_SET(_ev, (uintptr_t)tmp, EVFILT_USER, f, NOTE_FFNOP, 0, tmp);
        break;
    default: 
        break;
    }

    int ret = kevent(kp->fd, _ev, 1, NULL, 0, NULL);
    /* Workaround for STDIN_FILENO: it is actually pollable */
    if (tmp->type == M_SRC_TYPE_FD && tmp->fd_src.fd == STDIN_FILENO) {
        ret = 0;
    }
    
    /* Eventually free kqueue data if needed */
    if (flag == RM) {
        memhook._free(tmp->ev);
        tmp->ev = NULL;
        
        if (tmp->type == M_SRC_TYPE_PATH) {
            close(tmp->path_src.f.fd); // automatically close internally used FDs
            tmp->path_src.f.fd = -1;
        }
    }
    
    return ret;
}

int poll_init(poll_priv_t *priv) {
    GET_PRIV_DATA();
    kp->pevents = memhook._calloc(priv->max_events, sizeof(struct kevent));
    M_ALLOC_ASSERT(kp->pevents);
    return 0;
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

int poll_consume_sgn(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_sgn_t *sgn_msg) {
    return 0;
}

int poll_consume_tmr(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_tmr_t *tm_msg) {
    return 0;
}

int poll_consume_pt(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_path_t *pt_msg) {
    GET_PRIV_DATA();
    pt_msg->events = kp->pevents[idx].fflags;
    return 0;
}

int poll_consume_pid(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_pid_t *pid_msg) {
    GET_PRIV_DATA();
    pid_msg->events = kp->pevents[idx].fflags;
    return 0;
}

int poll_consume_task(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_task_t *task_msg) {
    GET_PRIV_DATA();
    task_msg->retval = src->task_src.retval;
    return 0;
}

int poll_consume_thresh(poll_priv_t *priv, const int idx, ev_src_t *src, m_evt_thresh_t *thresh_msg) {
    return 0;
}

int poll_get_fd(const poll_priv_t *priv) {
    GET_PRIV_DATA();
    return kp->fd;
}

int poll_clear(poll_priv_t *priv) {
    GET_PRIV_DATA();
    memhook._free(kp->pevents);
    kp->pevents = NULL;
    return 0;
}

int poll_destroy(poll_priv_t *priv) {
    GET_PRIV_DATA();
    poll_clear(priv);
    close(kp->fd);
    return 0;
}

int poll_notify_userevent(poll_priv_t *priv, ev_src_t *src) {
    GET_PRIV_DATA();
    struct kevent *_ev = (struct kevent *)src->ev;
    EV_SET(_ev, (uintptr_t)src, EVFILT_USER, 0, NOTE_FFNOP | NOTE_TRIGGER, 0, src);
    return kevent(kp->fd, _ev, 1, NULL, 0, NULL);
}