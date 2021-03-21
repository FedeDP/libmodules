#include "poll_priv.h"

/**********************************
 * Code related to event sources. *
 **********************************/

#define M_TASK_MAX_THREADS 16

static void src_priv_dtor(void *data);
static void *task_thread(void *data);

static int fdcmp(void *my_data, void *node_data);
static int tmrcmp(void *my_data, void *node_data);
static int sgncmp(void *my_data, void *node_data);
static int pathcmp(void *my_data, void *node_data);
static int pidcmp(void *my_data, void *node_data);
static int taskcmp(void *my_data, void *node_data);
static int threshcmp(void *my_data, void *node_data);

static m_bst_cmp src_cmp_map[] = {
        fdcmp,      // M_SRC_TYPE_PS we use internal pipe fd used for pubsub as module PS source
        fdcmp,      // M_SRC_TYPE_FD
        tmrcmp,     // M_SRC_TYPE_TMR
        sgncmp,     // M_SRC_TYPE_SGN
        pathcmp,    // M_SRC_TYPE_PATH
        pidcmp,     // M_SRC_TYPE_PID
        taskcmp,    // M_SRC_TYPE_TASK
        threshcmp,  // M_SRC_TYPE_THRESH
};
_Static_assert(sizeof(src_cmp_map) / sizeof(*src_cmp_map) == M_SRC_TYPE_END, "Undefined source compare function.");

const char *src_names[] = {
        "Priv",     // M_SRC_TYPE_PS is not exposed externally
        "Fds",      // M_SRC_TYPE_FD
        "Tmrs",     // M_SRC_TYPE_TMR
        "Sgns",     // M_SRC_TYPE_SGN
        "Paths",    // M_SRC_TYPE_PATH
        "Pids",     // M_SRC_TYPE_PID
        "Tasks",    // M_SRC_TYPE_TASK
        "Thresh",   // M_SRC_TYPE_THRESH
};
_Static_assert(sizeof(src_names) / sizeof(*src_names) == M_SRC_TYPE_END, "Undefined source name.");

static void src_priv_dtor(void *data) {
    ev_src_t *t = (ev_src_t *)data;

    /* If a fd is deregistered for a RUNNING module, stop polling on it */
    if (m_mod_is(t->mod, M_MOD_RUNNING)) {
        M_MOD_CTX(t->mod);
        poll_set_new_evt(&c->ppriv, t, RM);
    }

    /* Properly manage autoclose flag */
    if (t->flags & M_SRC_FD_AUTOCLOSE) {
        int fd = -1;
        switch (t->type) {
            case M_SRC_TYPE_PS: // M_SRC_TYPE_PS is used for pubsub_fd[0] in init_pubsub_fd()
            case M_SRC_TYPE_FD:
                fd = t->fd_src.fd;
                break;
            default:
                break;
        }
        if (fd != -1) {
            close(fd);
        }
    }

    if (t->flags & M_SRC_DUP) {
        switch (t->type) {
            case M_SRC_TYPE_PATH:
                memhook._free((void *)t->path_src.pt.path);
                break;
            default:
                break;
        }
    }

    if (t->flags & M_SRC_AUTOFREE) {
        memhook._free((void *)t->userptr);
    }

    memhook._free(t);
}

static void *task_thread(void *data) {
    ev_src_t *src = (ev_src_t *)data;
    M_MOD_CTX(src->mod);
    src->task_src.retval = src->task_src.tid.fn((void *)src->userptr);
    poll_notify_userevent(&c->ppriv, src);
    return NULL;
}

static int fdcmp(void *my_data, void *node_data) {
    ev_src_t *src = (ev_src_t *)node_data;
    int fd = *((int *)my_data);

    return fd - src->fd_src.fd;
}

static int tmrcmp(void *my_data, void *node_data) {
    ev_src_t *src = (ev_src_t *)node_data;
    const m_src_tmr_t *its = (const m_src_tmr_t *)my_data;

    return its->ms - src->tmr_src.its.ms;
}

static int sgncmp(void *my_data, void *node_data) {
    ev_src_t *src = (ev_src_t *)node_data;
    const m_src_sgn_t *sgs = (const m_src_sgn_t *)my_data;

    return sgs->signo - src->sgn_src.sgs.signo;
}

static int pathcmp(void *my_data, void *node_data) {
    ev_src_t *src = (ev_src_t *)node_data;
    const m_src_path_t *pt = (const m_src_path_t *)my_data;

    return strcmp(pt->path, src->path_src.pt.path);
}

static int pidcmp(void *my_data, void *node_data) {
    ev_src_t *src = (ev_src_t *)node_data;
    const m_src_pid_t *pid = (const m_src_pid_t *)my_data;

    return pid->pid - src->pid_src.pid.pid;
}

static int taskcmp(void *my_data, void *node_data) {
    ev_src_t *src = (ev_src_t *)node_data;
    const m_src_task_t *tid = (const m_src_task_t *)my_data;

    return tid->tid - src->task_src.tid.tid;
}

static int threshcmp(void *my_data, void *node_data) {
    ev_src_t *src = (ev_src_t *)node_data;
    const m_src_thresh_t *thr = (const m_src_thresh_t *)my_data;

    long double my_val = (long double)thr->activity_freq
                         + (long double)thr->inactive_ms;
    long double their_val = (long double)src->thresh_src.thr.activity_freq
                            + (long double)src->thresh_src.thr.inactive_ms;
    return my_val - their_val;
}

/** Private API **/

int init_src(m_mod_t *mod, m_src_types t) {
    mod->srcs[t] = m_bst_new(src_cmp_map[t], src_priv_dtor);
    if (!mod->srcs[t]) {
        return -ENOMEM;
    }
    return 0;
}

int register_src(m_mod_t *mod, m_src_types type, const void *src_data,
                         m_src_flags flags, const void *userptr) {
    M_MOD_ASSERT(mod);
    ev_src_t *src = memhook._calloc(1, sizeof(ev_src_t));
    M_ALLOC_ASSERT(src);

    src->flags = flags;
    src->userptr = userptr;
    src->type = type;
    src->mod = mod;

    /*
     * Same storage is used for all linux's internal fds, eg: for timerfd, signalfd...
     * as fd_src_t is always first struct field on linux.
     */
    src->fd_src.fd = -1;

    switch (type) {
        case M_SRC_TYPE_PS: // M_SRC_TYPE_PS is used for pubsub_fd[0] in init_pubsub_fd()
        case M_SRC_TYPE_FD: {
            fd_src_t *fd_src = &src->fd_src;
            int fd = *((int *)src_data);
            if (flags & M_SRC_DUP) {
                fd_src->fd = dup(fd);
                src->flags |= M_SRC_FD_AUTOCLOSE;
            } else {
                fd_src->fd = fd;
            }
            break;
        }
        case M_SRC_TYPE_TMR: {
            tmr_src_t *tm_src = &src->tmr_src;
            memcpy(&tm_src->its, src_data, sizeof(m_src_tmr_t));
            break;
        }
        case M_SRC_TYPE_SGN: {
            sgn_src_t *sgn_src = &src->sgn_src;
            memcpy(&sgn_src->sgs, src_data, sizeof(m_src_sgn_t));
            break;
        }
        case M_SRC_TYPE_PATH: {
            path_src_t *pt_src = &src->path_src;
            memcpy(&pt_src->pt, src_data, sizeof(m_src_path_t));
            if (flags & M_SRC_DUP) {
                pt_src->pt.path = mem_strdup(pt_src->pt.path);
            }
            break;
        }
        case M_SRC_TYPE_PID: {
            pid_src_t *pid_src = &src->pid_src;
            memcpy(&pid_src->pid, src_data, sizeof(m_src_pid_t));
            break;
        }
        case M_SRC_TYPE_TASK: {
            task_src_t *task_src = &src->task_src;
            memcpy(&task_src->tid, src_data, sizeof(m_src_task_t));
            break;
        }
        case M_SRC_TYPE_THRESH: {
            thresh_src_t *thresh_src = &src->thresh_src;
            memcpy(&thresh_src->thr, src_data, sizeof(m_src_thresh_t));
            break;
        }
        default:
            M_DEBUG("Wrong src type: %d\n", type);
            memhook._free(src);
            return -EINVAL;
    }

    int ret = m_bst_insert(mod->srcs[type], src);
    if (ret == 0) {
        fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
        /* If a src is registered at runtime, start receiving its events */
        if (m_mod_is(mod, M_MOD_RUNNING)) {
            M_MOD_CTX(mod);
            ret = poll_set_new_evt(&c->ppriv, src, ADD);

            /* For type task: create task thread */
            if (ret == 0 && src->type == M_SRC_TYPE_TASK) {
                ret = start_task(c, src);
            }
        }
        return !ret ? 0 : -errno;
    }
    memhook._free(src);
    return ret;
}

int deregister_src(m_mod_t *mod, m_src_types type, void *src_data) {
    M_MOD_ASSERT(mod);

    int ret = m_bst_remove(mod->srcs[type], src_data);
    if (ret == 0) {
        fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
    }
    return ret;
}

int start_task(m_ctx_t *c, ev_src_t *src) {
    if (!c->thpool) {
        c->thpool = m_thpool_new(M_TASK_MAX_THREADS, M_THPOOL_LAZY);
    }
    M_ALLOC_ASSERT(c->thpool);
    return m_thpool_add(c->thpool, task_thread, src);
}

/** Public API **/

_public_ int m_mod_src_register_fd(m_mod_t *mod, int fd, m_src_flags flags, const void *userptr) {
    M_PARAM_ASSERT(fd >= 0);

    return register_src(mod, M_SRC_TYPE_FD, &fd, flags, userptr);
}

_public_ int m_mod_src_deregister_fd(m_mod_t *mod, int fd) {
    M_PARAM_ASSERT(fd >= 0);

    return deregister_src(mod, M_SRC_TYPE_FD, (void *)&fd);
}

_public_ int m_mod_src_register_tmr(m_mod_t *mod, const m_src_tmr_t *its, m_src_flags flags, const void *userptr) {
    M_PARAM_ASSERT(its && its->ms > 0);

    return register_src(mod, M_SRC_TYPE_TMR, its, flags, userptr);
}

_public_ int m_mod_src_deregister_tmr(m_mod_t *mod, const m_src_tmr_t *its) {
    M_PARAM_ASSERT(its && its->ms > 0);

    return deregister_src(mod, M_SRC_TYPE_TMR, (void *)its);
}

_public_ int m_mod_src_register_sgn(m_mod_t *mod, const m_src_sgn_t *sgs, m_src_flags flags, const void *userptr) {
    M_PARAM_ASSERT(sgs && sgs->signo > 0);

    return register_src(mod, M_SRC_TYPE_SGN, sgs, flags, userptr);
}

_public_ int m_mod_src_deregister_sgn(m_mod_t *mod, const m_src_sgn_t *sgs) {
    M_PARAM_ASSERT(sgs && sgs->signo > 0);

    return deregister_src(mod, M_SRC_TYPE_SGN, (void *)sgs);
}

_public_ int m_mod_src_register_path(m_mod_t *mod, const m_src_path_t *pt, m_src_flags flags, const void *userptr) {
    M_PARAM_ASSERT(pt);
    M_PARAM_ASSERT(pt->path && strlen(pt->path));
    M_PARAM_ASSERT(pt->events > 0);

    return register_src(mod, M_SRC_TYPE_PATH, pt, flags, userptr);
}

_public_ int m_mod_src_deregister_path(m_mod_t *mod, const m_src_path_t *pt) {
    M_PARAM_ASSERT(pt);
    M_PARAM_ASSERT(pt->path && strlen(pt->path));

    return deregister_src(mod, M_SRC_TYPE_PATH, (void *)pt);
}

_public_ int m_mod_src_register_pid(m_mod_t *mod, const m_src_pid_t *pid, m_src_flags flags, const void *userptr) {
    M_PARAM_ASSERT(pid && pid->pid > 0);

    return register_src(mod, M_SRC_TYPE_PID, pid, flags, userptr);
}

_public_ int m_mod_src_deregister_pid(m_mod_t *mod, const m_src_pid_t *pid) {
    M_PARAM_ASSERT(pid && pid->pid > 0);

    return deregister_src(mod, M_SRC_TYPE_PID, (void *)pid);
}

_public_ int m_mod_src_register_task(m_mod_t *mod, const m_src_task_t *tid, m_src_flags flags, const void *userptr) {
    M_PARAM_ASSERT(tid && tid->fn);

    return register_src(mod, M_SRC_TYPE_TASK, tid, flags | M_SRC_ONESHOT, userptr); // force ONESHOT flag
}

_public_ int m_mod_src_deregister_task(m_mod_t *mod, const m_src_task_t *tid) {
    M_PARAM_ASSERT(tid);

    /* Tasks cannot be deregistered */
    return -EPERM;
}

_public_ int m_mod_src_register_thresh(m_mod_t *mod, const m_src_thresh_t *thr, m_src_flags flags, const void *userptr) {
    M_PARAM_ASSERT(thr && (thr->activity_freq > 0 || thr->inactive_ms > 0));

    return register_src(mod, M_SRC_TYPE_THRESH, thr, flags | M_SRC_ONESHOT, userptr); // force ONESHOT flag
}

_public_ int m_mod_src_deregister_thresh(m_mod_t *mod, const m_src_thresh_t *thr) {
    M_PARAM_ASSERT(thr && (thr->activity_freq > 0 || thr->inactive_ms > 0));

    return deregister_src(mod, M_SRC_TYPE_THRESH, (void *)thr);
}