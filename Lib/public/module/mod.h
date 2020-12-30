#pragma once

#include "commons.h"

/* Module interface functions */

/* Module registration */
_public_ int m_mod_register(const char *name, m_ctx_t *c, m_mod_t **mod, const userhook_t *hook, 
                            const m_mod_flags flags, const void *userdata);
_public_ int m_mod_deregister(m_mod_t **mod);

/* Retrieve module context */
_public_ _pure_ m_ctx_t *m_mod_ctx(const m_mod_t *mod);

/* Retrieve module name */
_public_ _pure_ const char *m_mod_name(const m_mod_t *mod);

/* Module state getters */
_public_ _pure_ bool m_mod_is(const m_mod_t *mod, const mod_states st);

/* Module state setters */
_public_ int m_mod_start(m_mod_t *mod);
_public_ int m_mod_pause(m_mod_t *mod);
_public_ int m_mod_resume(m_mod_t *mod);
_public_ int m_mod_stop(m_mod_t *mod);

/* Module generic functions */
_public_ __attribute__((format (printf, 2, 3))) int m_mod_log(const m_mod_t *mod, const char *fmt, ...);
_public_ int m_mod_dump(const m_mod_t *mod);
_public_ _pure_ int m_mod_stats(const m_mod_t *mod, stats_t *stats);

_public_ int m_mod_set_userdata(m_mod_t *mod, const void *userdata);
_public_ const void *m_mod_get_userdata(const m_mod_t *mod);

_public_ int m_mod_ref(const m_mod_t *mod, const char *name, m_mod_t **modref);
_public_ int m_mod_unref(const m_mod_t *mod, m_mod_t **modref);

/* Module PubSub interface */
_public_ int m_mod_become(m_mod_t *mod, const recv_cb new_recv);
_public_ int m_mod_unbecome(m_mod_t *mod);

_public_ int m_mod_tell(m_mod_t *mod, const m_mod_t *recipient, const void *message, const m_ps_flags flags);
_public_ int m_mod_publish(m_mod_t *mod, const char *topic, const void *message, const m_ps_flags flags);
_public_ int m_mod_broadcast(m_mod_t *mod, const void *message, const m_ps_flags flags);
_public_ int m_mod_poisonpill(m_mod_t *mod, const m_mod_t *recipient);

/* Event Sources management */
_public_ int m_mod_register_fd(m_mod_t *mod, const int fd, const m_src_flags flags, const void *userptr);
_public_ int m_mod_deregister_fd(m_mod_t *mod, const int fd);

_public_ int m_mod_register_tmr(m_mod_t *mod, const mod_tmr_t *its, const m_src_flags flags, const void *userptr);
_public_ int m_mod_deregister_tmr(m_mod_t *mod, const mod_tmr_t *its);

_public_ int m_mod_register_sgn(m_mod_t *mod, const mod_sgn_t *its, const m_src_flags flags, const void *userptr);
_public_ int m_mod_deregister_sgn(m_mod_t *mod, const mod_sgn_t *its);

_public_ int m_mod_register_path(m_mod_t *mod, const mod_path_t *its, const m_src_flags flags, const void *userptr);
_public_ int m_mod_deregister_path(m_mod_t *mod, const mod_path_t *its);

_public_ int m_mod_register_pid(m_mod_t *mod, const mod_pid_t *pid, const m_src_flags flags, const void *userptr);
_public_ int m_mod_deregister_pid(m_mod_t *mod, const mod_pid_t *pid);

_public_ int m_mod_register_task(m_mod_t *mod, const mod_task_t *tid, const m_src_flags flags, const void *userptr);
_public_ int m_mod_deregister_task(m_mod_t *mod, const mod_task_t *tid);

_public_ int m_mod_register_sub(m_mod_t *mod, const char *topic, const m_src_flags flags, const void *userptr);
_public_ int m_mod_deregister_sub(m_mod_t *mod, const char *topic);

/* Generic event source registering functions */
#define m_mod_register_src(mod, X, flags, userptr) _Generic((X) + 0, \
    int: m_mod_register_fd, \
    char *: m_mod_register_sub, \
    mod_tmr_t *: m_mod_register_tmr, \
    mod_sgn_t *: m_mod_register_sgn, \
    mod_path_t *: m_mod_register_path, \
    mod_pid_t *: m_mod_register_pid, \
    mod_task_t *: m_mod_register_task \
    )(mod, X, flags, userptr)

#define m_mod_deregister_src(mod, X) _Generic((X) + 0, \
    int: m_mod_deregister_fd, \
    char *: m_mod_deregister_sub, \
    mod_tmr_t *: m_mod_deregister_tmr, \
    mod_sgn_t *: m_mod_deregister_sgn, \
    mod_path_t *: m_mod_deregister_path, \
    mod_pid_t *: m_mod_deregister_pid, \
    mod_task_t *: m_mod_deregister_task \
    )(mod, X)