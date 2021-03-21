#include "mod.h"
#include "ctx.h"
#include "fs_priv.h"

/**************************************
 * Code related to contexts handling. *
 **************************************/

static void ctx_dtor(void *data);
static void default_logger(const m_mod_t *mod, const char *fmt, va_list args);
static int loop_start(m_ctx_t *c, int max_events);
static uint8_t loop_stop(m_ctx_t *c);
static inline int loop_quit(m_ctx_t *c, uint8_t quit_code);
static int recv_events(m_ctx_t *c, int timeout);
static int m_ctx_loop_events(m_ctx_t *c, int max_events);

static void ctx_dtor(void *data) {
    M_DEBUG("Destroying context.\n");
    m_ctx_t *context = (m_ctx_t *)data;
    m_map_free(&context->modules);
    poll_destroy(&context->ppriv);
    memhook._free(context->ppriv.data);
    memhook._free(context->fs_root);

    if (context->flags & M_CTX_NAME_AUTOFREE) {
        memhook._free((void *)context->name);
    }

    if (context->flags & M_CTX_USERDATA_AUTOFREE) {
        memhook._free((void *)context->userdata);
    }
}

static void default_logger(const m_mod_t *mod, const char *fmt, va_list args) {
    if (mod) {
        printf("[%s]|%s|: ", mod->ctx->name, mod->name);
    }
    vprintf(fmt, args);
}

static int loop_start(m_ctx_t *c, int max_events) {
    c->ppriv.max_events = max_events;
    int ret = poll_init(&c->ppriv);
    if (ret == 0) {
        /* Initialize fuse fs if requested */
        if (c->fs_root && strlen(c->fs_root)) {
            int fs_ret = fs_init(c);
            if (fs_ret != 0) {
                M_WARN("Failed to initialize fuse fs: %s\n", strerror(-fs_ret));
            }
        }

        fetch_ms(&c->stats.looping_start_time, NULL);
        c->state = M_CTX_LOOPING;
        c->quit = false;
        c->quit_code = 0;
        
        /* Eventually start any IDLE module */
        m_map_iterate(c->modules, evaluate_module, NULL);

        /* Tell every RUNNING module that loop is started */
        tell_system_pubsub_msg(NULL, c, M_PS_CTX_STARTED, NULL, NULL);
    }
    return ret;
}

static uint8_t loop_stop(m_ctx_t *c) {
    /* Tell every module that loop is stopped */
    tell_system_pubsub_msg(NULL, c, M_PS_CTX_STOPPED, NULL, NULL);

    /* Flush pubsub msg to avoid memleaks */
    m_map_iterate(c->modules, flush_pubsub_msgs, NULL);
    
    /* Destroy FS */
    fs_end(c);

    poll_clear(&c->ppriv);

    /* Destroy thpool eventually waiting on currently running tasks */
    m_thpool_free(&c->thpool, false);

    c->ppriv.max_events = 0;
    c->state = M_CTX_IDLE;
    c->stats.looping_start_time = 0;
    c->stats.recv_msgs = 0;
    c->stats.idle_time = 0;

    int ret = c->quit_code;

    /*
     * ctx cannot be deregistered while looping,
     * thus even if all modules were deregistered during the loop,
     * and last module's tried to call m_ctx_deregister(), it returned -EPERM.
     * Gracefully deregister it now.
     */
    if (m_map_len(c->modules) == 0 && !(c->flags & M_CTX_PERSIST)) {
        m_ctx_deregister(&c);
    }
    return ret;
}

static int loop_quit(m_ctx_t *c, uint8_t quit_code) {
    c->quit = true;
    c->quit_code = quit_code;
    return 0;
}

static int recv_events(m_ctx_t *c, int timeout) {
    static uint64_t last_time_called;

    if (c->stats.recv_msgs == 0) {
        // First time entering: (re)start counter
        fetch_ms(&last_time_called, NULL);
    }

    int err;
    int recved = 0;

    errno = 0;
    const int nfds = poll_wait(&c->ppriv, timeout);
    err = errno; // store any errno happened in poll_wait

    // Store idling time stat
    uint64_t now;
    fetch_ms(&now, NULL);
    c->stats.idle_time += now - last_time_called;

    for (int i = 0; i < nfds && !err; i++) {
        ev_src_t *p = poll_recv(&c->ppriv, i);
        if (p && p->type == M_SRC_TYPE_FD && p->mod == NULL) {
            /* Received from fuse */
            fs_process(c);
            recved++;
            continue;
        }
        
        if (p && p->mod) {
            /*
             * Keep a reference on mod, to avoid that
             * a m_mod_deregister() call by user callback
             * invalidates our pointer.
             */
            m_mod_t *mod = m_mem_ref(p->mod);
            m_evt_t *msg = new_evt(p->type);
            if (msg) {
                M_DEBUG("'%s' received %u type msg.\n", mod->name, msg->type);
                switch (msg->type) {
                case M_SRC_TYPE_FD:
                    /* Received from FD */
                    msg->fd_evt = m_mem_new(sizeof(*msg->fd_evt), NULL);
                    msg->fd_evt->fd = p->fd_src.fd;
                    break;
                case M_SRC_TYPE_PS: {
                    ps_priv_t *ps_msg;
                    /* Received on pubsub interface */
                    if (read(p->fd_src.fd, (void **)&ps_msg, sizeof(ps_priv_t *)) != sizeof(ps_priv_t *)) {
                        M_WARN("Failed to read message: %s\n", strerror(errno));
                    } else {
                        msg->ps_evt = &ps_msg->msg;
                        p = ps_msg->sub;            // Use real event source, ie: topic subscription if any
                    }
                    break;
                }
                case M_SRC_TYPE_SGN:
                    msg->sgn_evt = m_mem_new(sizeof(*msg->sgn_evt), NULL);
                    if (poll_consume_sgn(&c->ppriv, i, p, msg->sgn_evt) == 0) {
                        msg->sgn_evt->signo = p->sgn_src.sgs.signo;
                    }
                    break;
                case M_SRC_TYPE_TMR:
                    msg->tmr_evt = m_mem_new(sizeof(*msg->tmr_evt), NULL);
                    if (poll_consume_tmr(&c->ppriv, i, p,  msg->tmr_evt) == 0) {
                        msg->tmr_evt->ms = p->tmr_src.its.ms;
                    }
                    break;
                case M_SRC_TYPE_PATH:
                    msg->path_evt = m_mem_new(sizeof(*msg->path_evt), NULL);
                    if (poll_consume_pt(&c->ppriv, i, p, msg->path_evt) == 0) {
                        msg->path_evt->path = p->path_src.pt.path;
                    }
                    break;
                case M_SRC_TYPE_PID:
                    msg->pid_evt = m_mem_new(sizeof(*msg->pid_evt), NULL);
                    if (poll_consume_pid(&c->ppriv, i, p, msg->pid_evt) == 0) {
                        msg->pid_evt->pid = p->pid_src.pid.pid;
                    }
                    break;
                case M_SRC_TYPE_TASK:
                    msg->task_evt = m_mem_new(sizeof(*msg->task_evt), NULL);
                    if (poll_consume_task(&c->ppriv, i, p, msg->task_evt) == 0) {
                        msg->task_evt->tid = p->task_src.tid.tid;
                    }
                    break;
                case M_SRC_TYPE_THRESH:
                    msg->thresh_evt = m_mem_new(sizeof(*msg->thresh_evt), NULL);
                    if (poll_consume_thresh(&c->ppriv, i, p, msg->thresh_evt) == 0) {
                        msg->thresh_evt->inactive_ms = p->thresh_src.alarm.inactive_ms;
                        msg->thresh_evt->activity_freq = p->thresh_src.alarm.activity_freq;
                    }
                    break;
                default:
                    M_DEBUG("Unmanaged src %d.\n", msg->type);
                    break;
                }
            }
            err = errno; // Store any errno happend while consuming events

            if (err == 0) {
                /*
                 * All messages share same address inside union.
                 * In this case, check that any message was actually received,
                 * and it was from a know source type.
                 * It should never happen though.
                 */
                if (msg->fd_evt) {
                    recved++;
                    if (msg->type != M_SRC_TYPE_PS || msg->ps_evt->type != M_PS_MOD_POISONPILL) {
                        run_pubsub_cb(mod, msg, p);
                    } else {
                        M_DEBUG("PoisonPilling '%s'.\n", mod->name);
                        stop(mod, true);
                    }
                }
                
                /* Remove it if it was a oneshot event */
                if (p && p->flags & M_SRC_ONESHOT) {
                    if (p->type != M_SRC_TYPE_PS) {
                        m_bst_remove(mod->srcs[p->type], p);
                    } else {
                        m_map_remove(mod->subscriptions, p->ps_src.topic);
                    }
                }
            }
            m_mem_unref(mod);
        } else {
            /* Forward error to below handling code */
            err = EAGAIN;
        }
    }

    if (recved > 0 && err == 0) {
        m_map_iterate(c->modules, evaluate_module, NULL);
        c->stats.recv_msgs += recved;
    } else if (err) {
        /* Quit and return < 0 only for real errors */
        if (err != EINTR && err != EAGAIN) {
            M_WARN("Ctx '%s' loop error: %s.\n", c->name, strerror(err));
            loop_quit(c, err);
            recved = -1; // error!
        }
    }

    fetch_ms(&last_time_called, NULL);
    return recved;
}

static int m_ctx_loop_events(m_ctx_t *c, int max_events) {
    M_CTX_ASSERT(c);
    M_PARAM_ASSERT(max_events > 0);
    M_ASSERT(c->state == M_CTX_IDLE, "Context already looping.", -EINVAL);

    int ret = loop_start(c, max_events);
    if (ret == 0) {
        while (!c->quit && c->stats.running_modules > 0) {
            recv_events(c, -1);
        }
        return loop_stop(c);
    }
    return ret;
}

/** Private API **/

int ctx_new(const char *ctx_name, m_ctx_t **c, m_ctx_flags flags, const void *userdata) {
    pthread_mutex_lock(&mx);

    M_DEBUG("Creating context '%s'.\n", ctx_name);

    *c = m_mem_new(sizeof(m_ctx_t), ctx_dtor);
    M_ALLOC_ASSERT(*c);

    (*c)->flags = flags;
    (*c)->userdata = userdata;
    (*c)->logger = default_logger;
    (*c)->modules = m_map_new(0, mem_dtor);

    if ((*c)->flags & M_CTX_NAME_DUP) {
        (*c)->flags |= M_CTX_NAME_AUTOFREE;
        (*c)->name = mem_strdup(ctx_name);
    } else {
        (*c)->name = ctx_name;
    }

    int ret = -ENOMEM;
    if ((*c)->modules && (*c)->name) {
        ret = m_map_put(ctx, (*c)->name, *c);
        if (ret == 0) {
            ret = poll_create(&(*c)->ppriv);
        }
    }

    if (ret != 0) {
        /* Map_remove automatically unref ctx for us */
        if (m_map_remove(ctx, (*c)->name) != 0) {
            m_mem_unref(*c);
        }
        *c = NULL;
    }

    pthread_mutex_unlock(&mx);
    return ret;
}

m_ctx_t *check_ctx(const char *ctx_name) {
    pthread_mutex_lock(&mx);
    m_ctx_t *context = m_map_get(ctx, ctx_name);
    pthread_mutex_unlock(&mx);
    return context;
}

void inline ctx_logger(const m_ctx_t *c, const m_mod_t *mod, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    c->logger(mod, fmt, args);
    va_end(args);
}

/** Public API **/

_public_ int m_ctx_register(const char *ctx_name, m_ctx_t **c, m_ctx_flags flags, const void *userdata) {
    M_PARAM_ASSERT(ctx_name);
    M_PARAM_ASSERT(c);
    M_PARAM_ASSERT(!*c);

    if (check_ctx(ctx_name)) {
        return -EEXIST;
    }
    return ctx_new(ctx_name, c, flags, userdata);
}

_public_ int m_ctx_deregister(m_ctx_t **c) {
    M_PARAM_ASSERT(c && *c && (*c)->state == M_CTX_IDLE);

    (*c)->state = M_CTX_ZOMBIE;

    pthread_mutex_lock(&mx);
    int ret = m_map_remove(ctx, (*c)->name);
    pthread_mutex_unlock(&mx);
    *c = NULL;
    return ret;
}

_public_ int m_ctx_set_logger(m_ctx_t *c, m_log_cb logger) {
    M_CTX_ASSERT(c);
    M_PARAM_ASSERT(logger);
    
    c->logger = logger;
    return 0;
}

_public_ int m_ctx_loop(m_ctx_t *c) {
    return m_ctx_loop_events(c, M_CTX_DEFAULT_EVENTS);
}

_public_ int m_ctx_quit(m_ctx_t *c, uint8_t quit_code) {
    M_CTX_ASSERT(c);
    M_ASSERT(c->state == M_CTX_LOOPING, "Context not looping.", -EINVAL);
       
    return loop_quit(c, quit_code);
}

_public_ int m_ctx_fd(const m_ctx_t *c) {
    M_CTX_ASSERT(c);

    return dup(poll_get_fd(&c->ppriv));
}

_public_ int m_ctx_dispatch(m_ctx_t *c) {
    M_CTX_ASSERT(c);

    if (c->state == M_CTX_IDLE) {
        /* Ok, start now */
        return loop_start(c, M_CTX_DEFAULT_EVENTS);
    }
    
    if (c->quit || c->stats.running_modules == 0) {
        /* We are stopping! */
        return loop_stop(c);
    }

    /* Recv new events, no timeout */
    return recv_events(c, 0);
}

_public_ int m_ctx_dump(const m_ctx_t *c) {
    M_CTX_ASSERT(c);

    ctx_logger(c, NULL, "{\n");
    ctx_logger(c, NULL, "\t\"Name\": \"%s\",\n", c->name);
    if (c->flags) {
        ctx_logger(c, NULL, "\t\"Flags\": \"%#x\",\n", c->flags);
    }
    if (c->userdata) {
        ctx_logger(c, NULL, "\t\"UP\": \"%p\",\n", c->userdata);
    }
    if (c->fs_root && strlen(c->fs_root)) {
        ctx_logger(c, NULL, "\t\t\"Fs_root\": \"%s\",\n", c->fs_root);
    }
    ctx_logger(c, NULL, "\t\"State\": {\n");
    ctx_logger(c, NULL, "\t\t\"Quit\": %d,\n", c->quit);
    ctx_logger(c, NULL, "\t\t\"Looping\": %d\n", c->state == M_CTX_LOOPING);
    ctx_logger(c, NULL, "\t},\n");

    uint64_t now;
    fetch_ms(&now, NULL);
    uint64_t total_looping_time = now - c->stats.looping_start_time;
    uint64_t total_busy_time = total_looping_time - c->stats.idle_time;
    ctx_logger(c, NULL, "\t\"Stats\": {\n");
    ctx_logger(c, NULL, "\t\t\"Looping_since\": %" PRIu64 ",\n", c->stats.looping_start_time);
    ctx_logger(c, NULL, "\t\t\"Looping_time\": %" PRIu64 ",\n", total_looping_time);
    ctx_logger(c, NULL, "\t\t\"Idle_time\": %" PRIu64 ",\n", c->stats.idle_time);
    ctx_logger(c, NULL, "\t\t\"Busy_time\": %" PRIu64 ",\n", total_busy_time);
    ctx_logger(c, NULL, "\t\t\"Recv_events\": %" PRIu64 ",\n", c->stats.recv_msgs);
    ctx_logger(c, NULL, "\t\t\"Action_freq\": %lf,\n", (double)c->stats.recv_msgs / total_looping_time);
    ctx_logger(c, NULL, "\t\t\"Running_modules\": %" PRIu64 "\n", c->stats.running_modules);
    ctx_logger(c, NULL, "\t},\n");

    ctx_logger(c, NULL, "\t\"Modules\": [\n");
    int i = 0;
    m_itr_foreach(c->modules, {
        const char *mod_name = m_map_itr_get_key(itr);
        const m_mod_t *mod = m_itr_get(itr);
        ctx_logger(c, NULL, "\t\t\"%s\"%c\n", mod_name, ++i < m_map_len(c->modules) ? ',' : ' ');
    });
    ctx_logger(c, NULL, "\t]\n");
    ctx_logger(c, NULL, "}\n");
    return 0;
}

_public_ int m_ctx_stats(const m_ctx_t *c, m_ctx_stats_t *stats) {
    M_CTX_ASSERT(c);
    M_PARAM_ASSERT(c->state == M_CTX_LOOPING);
    M_PARAM_ASSERT(stats);

    uint64_t now;
    fetch_ms(&now, NULL);

    stats->recv_msgs = c->stats.recv_msgs;
    stats->num_modules = m_map_len(c->modules);
    stats->running_modules = c->stats.running_modules;
    stats->total_idle_time = c->stats.idle_time;
    stats->total_looping_time = now - c->stats.looping_start_time;

    uint64_t active_time = stats->total_looping_time - stats->total_idle_time;
    stats->activity_freq = (double)active_time / stats->total_looping_time;
    return 0;
}

_public_ const char *m_ctx_name(const m_ctx_t *c) {
    M_RET_ASSERT(c, NULL);
    M_RET_ASSERT(c->state != M_CTX_ZOMBIE, NULL);

    return c->name;
}

_public_ const void *m_ctx_userdata(const m_ctx_t *c) {
    M_RET_ASSERT(c, NULL);
    M_RET_ASSERT(c->state != M_CTX_ZOMBIE, NULL);

    return c->userdata;
}