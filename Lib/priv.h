#pragma once

#include <stdlib.h>
#include <regex.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h> // PRIu64
#include "mem.h"
#include "thpool.h"
#include "itr.h"
#include "ctx.h"
#include "mod.h"

#define unlikely(x)     __builtin_expect((x),0)
#define _weak_          __attribute__((weak))
#define _public_        __attribute__ ((visibility("default")))

#define M_CTX_DEFAULT_EVENTS   64
#define M_CTX_DEFAULT  "libmodule"

#ifndef NDEBUG
    #define M_DEBUG printf("| D | %s@%s:%d | ", M_CTX_DEFAULT, __func__, __LINE__); printf
#else
    #define M_DEBUG (void)
#endif

#define M_WARN printf("| W | %s@%s:%d | ", M_CTX_DEFAULT, __func__, __LINE__); printf

#define M_ASSERT(cond, msg, ret)    if (unlikely(!(cond))) { M_DEBUG("%s\n", msg); return ret; }

#define M_RET_ASSERT(cond, ret)     M_ASSERT(cond, "("#cond ") condition failed.", ret) 

#define M_ALLOC_ASSERT(cond)        M_RET_ASSERT(cond, -ENOMEM)
#define M_PARAM_ASSERT(cond)        M_RET_ASSERT(cond, -EINVAL)

#define M_CTX_ASSERT(c) \
    M_PARAM_ASSERT(c); \
    M_RET_ASSERT(c->state != M_CTX_ZOMBIE, -EACCES)

#define M_MOD_ASSERT(mod) \
    M_PARAM_ASSERT(mod); \
    M_RET_ASSERT(!m_mod_is(mod, M_MOD_ZOMBIE), -EACCES)

#define M_MOD_ASSERT_PERM(mod, perm) \
    M_MOD_ASSERT(mod); \
    M_RET_ASSERT(!(mod->flags & perm), -EPERM)
    
#define M_MOD_ASSERT_STATE(mod, state) \
    M_MOD_ASSERT(mod); \
    M_RET_ASSERT(m_mod_is(mod, state), -EACCES)

#define M_MOD_CTX(mod)    m_ctx_t *c = mod->ctx;

/* Struct that holds fds to self_t mapping for poll plugin */
typedef struct {
    int fd;
} fd_src_t;

/* Struct that holds timers to self_t mapping for poll plugin */
typedef struct {
#ifdef __linux__
    fd_src_t f;
#endif
    m_src_tmr_t its;
} tmr_src_t;

/* Struct that holds signals to self_t mapping for poll plugin */
typedef struct {
#ifdef __linux__
    fd_src_t f;
#endif
    m_src_sgn_t sgs;
} sgn_src_t;

/* Struct that holds paths to self_t mapping for poll plugin */
typedef struct {
    fd_src_t f; // in kqueue EVFILT_VNODE: open(path) is needed. Thus a fd is needed too.
    m_src_path_t pt;
} path_src_t;

/* Struct that holds pids to self_t mapping for poll plugin */
typedef struct {
#ifdef __linux__
    fd_src_t f;
#endif
    m_src_pid_t pid;
} pid_src_t;

/* Struct that holds task to self_t mapping for poll plugin */
typedef struct {
#ifdef __linux__
    fd_src_t f;
#endif
    m_src_task_t tid;
    pthread_t th;
    int retval;
} task_src_t;

/* Struct that holds thresh to self_t mapping for poll plugin */
typedef struct {
#ifdef __linux__
    fd_src_t f;
#endif
    m_src_thresh_t thr;
    m_src_thresh_t alarm;
} thresh_src_t;

/* Struct that holds pubsub subscriptions source data */
typedef struct {
    regex_t reg;
    const char *topic;
} ps_src_t;

/* Struct that holds generic "event source" data */
typedef struct {
    union {
        ps_src_t     ps_src;
        fd_src_t     fd_src;
        tmr_src_t    tmr_src;
        sgn_src_t    sgn_src;
        path_src_t   path_src;
        pid_src_t    pid_src;
        task_src_t   task_src;
        thresh_src_t thresh_src;
    };
    m_src_types type;
    m_src_flags flags;
    void *ev;                               // poll plugin defined data structure
    m_mod_t *mod;                           // ptr needed to map an event source to a module in poll_plugin
    const void *userptr;
} ev_src_t;

/* Struct that holds pubsub messaging, private */
typedef struct {
    m_evt_ps_t msg;
    m_ps_flags flags;
    ev_src_t *sub;
} ps_priv_t;

typedef struct {
    void *data;                             // Context's poll priv data (depends upon poll_plugin)
    int max_events;                         // Max number of returned events for poll_plugin
} poll_priv_t;

typedef struct {
    uint64_t registration_time;
    uint64_t last_seen;
    uint64_t action_ctr;
    uint64_t sent_msgs;
    uint64_t recv_msgs;
} mod_stats_t;

typedef struct {
    uint64_t looping_start_time;
    uint64_t idle_time;
    uint64_t recv_msgs;
    uint64_t running_modules;
} ctx_stats_t;

/* Ctx states */
typedef enum {
    M_CTX_IDLE,
    M_CTX_LOOPING,
    M_CTX_ZOMBIE,
} m_ctx_states;

/* Struct that holds data for each module */
/*
 * MEM-REFS for mod:
 * + 1 because it is registered
 * + 1 for each m_mod_ref() called on it
 * + 1 for each PS message sent (ie: message's sender is a new reference for sender)
 * + 1 for each fs open() call on it
 * Moreover, a reference is held while retrieving an event for the module and calling its on_evt() cb,
 * to avoid user calling m_mod_deregister() and invalidating our pointer.
 */
struct _mod {
    m_mod_states state;                     // module's state
    m_mod_flags flags;                      // Module's flags
    int pubsub_fd[2];                       // In and Out pipe for pubsub msg
    mod_stats_t stats;                      // Module's stats
    m_mod_hook_t hook;                      // module's user defined callbacks
    m_stack_t *recvs;                       // Stack of recv functions for module_become/unbecome (stack of funpointers)
    const void *userdata;                   // module's user defined data
    void *fs;                               // FS module priv data. NULL if unsupported
    const char *name;                       // module's name
    const char *local_path;                 // For runtime loaded modules: path of module
    m_bst_t *srcs[M_SRC_TYPE_END];          // module's event sources
    m_map_t *subscriptions;                 // module's subscriptions (map of ev_src_t*)
    m_queue_t *stashed;                     // module's stashed messages
    m_ctx_t *ctx;                           // Module's ctx
};

/* Struct that holds data for main context */
/*
 * MEM-REFS for ctx:
 * + 1 because it is registered
 * + 1 for each module registered in the context
 *      (thus it won't be actually destroyed until any module is inside it)
 */
struct _ctx {
    const char *name;
    m_ctx_states state;
    bool quit;                              // Context's quit flag
    uint8_t quit_code;                      // Context's quit code, returned by modules_ctx_loop()
    m_log_cb logger;                        // Context's log callback
    m_map_t *modules;                       // Context's modules
    poll_priv_t ppriv;                      // Priv data for poll_plugin implementation
    m_ctx_flags flags;                      // Context's flags
    char *fs_root;                          // Context's fuse FS root. Null if unsupported
    void *fs;                               // FS context handler. Null if unsupported
    ctx_stats_t stats;                      // Context' stats
    m_thpool_t  *thpool;                    // thpool for M_SRC_TYPE_TASK srcs
    const void *userdata;                   // Context's user defined data
};

/* Defined in mod.c */
int evaluate_module(void *data, const char *key, void *value);
int start(m_mod_t *mod, bool starting);
int stop(m_mod_t *mod, bool stopping);

/* Defined in ctx.c */
int ctx_new(const char *ctx_name, m_ctx_t **c, m_ctx_flags flags, const void *userdata);
m_ctx_t *check_ctx(const char *ctx_name);
void ctx_logger(const m_ctx_t *c, const m_mod_t *mod, const char *fmt, ...);

/* Defined in pubsub.c */
int tell_system_pubsub_msg(const m_mod_t *recipient, m_ctx_t *c, m_ps_types type, 
                           m_mod_t *sender, const char *topic);
int flush_pubsub_msgs(void *data, const char *key, void *value);
void run_pubsub_cb(m_mod_t *mod, m_evt_t *msg, const ev_src_t *src);

/* Defined in utils.c */
char *mem_strdup(const char *s);
void fetch_ms(uint64_t *val, uint64_t *ctr);
m_evt_t *new_evt(m_src_types type);

/* Defined in map.c */
void *m_map_peek(const m_map_t *m);

/* Defined in mem.c; used internally as dtor cb for structs APIs userptr, when it is memory ref counted */
void mem_dtor(void *src);

/* Defined in src.c */
extern const char *src_names[];
int init_src(m_mod_t *mod, m_src_types t);
int register_src(m_mod_t *mod, m_src_types type, const void *src_data,
                 m_src_flags flags, const void *userptr);
int deregister_src(m_mod_t *mod, m_src_types type, void *src_data);
int start_task(m_ctx_t *c, ev_src_t *src);

/* Gglobal variables are defined in main.c */
extern m_map_t *ctx;
extern m_memhook_t memhook;
extern pthread_mutex_t mx;