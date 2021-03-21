#include "mod.h"
#include "poll_priv.h"

/******************************************
 * Code related to Actor-like PubSub API. *
 ******************************************/

// PRIO_SIZE -> { lowest, low, norm, high, highest }
#define M_SRC_PS_PRIO_SIZE       __builtin_ctz((M_SRC_PS_PRIO_HIGHEST << 1) / M_SRC_PS_PRIO_LOWEST)
#define M_SRC_PS_PRIO_MASK       ~(M_SRC_PS_PRIO_HIGHEST << 1)

static void subscribtions_dtor(void *data);
static ev_src_t *fetch_sub(m_mod_t *mod, const char *topic);
static int tell_if(void *data, const char *key, void *value);
static ps_priv_t *alloc_ps_msg(const ps_priv_t *msg, ev_src_t *sub);
static int tell_global(void *data, const char *key, void *value);
static void ps_msg_dtor(void *data);
static int get_prio_flag(m_src_flags flag);
static int tell_subscribers(void *data, const char *key, void *value);
static int tell_pubsub_msg(ps_priv_t *m, const m_mod_t *recipient, m_ctx_t *c);
static int send_msg(m_mod_t *mod, const m_mod_t *recipient, const char *topic, 
                    const void *message, m_ps_flags flags);

extern int fs_notify(m_mod_t *mod, const m_evt_t *msg);

static void subscribtions_dtor(void *data) {
    ev_src_t *sub = (ev_src_t *)data;
    regfree(&sub->ps_src.reg);
    if (sub->flags & M_SRC_DUP) {
        memhook._free((void *)sub->ps_src.topic);
    }
    if (sub->flags & M_SRC_AUTOFREE) {
        memhook._free((void *)sub->userptr);
    }
}

static ev_src_t *fetch_sub(m_mod_t *mod, const char *topic) {    
    /* Check if module is directly subscribed to topic */
    ev_src_t *sub = m_map_get(mod->subscriptions, topic);
    if (sub) {        
        goto found;
    }
    
    /* Check if any stored subscriptions is a regex that matches topic */
    m_itr_foreach(mod->subscriptions, {
        sub = m_itr_get(itr);
        /* Execute regular expression */
        int ret = regexec(&sub->ps_src.reg, topic, 0, NULL, 0);
        if (!ret) {
            memhook._free(itr);
            goto found;
        }
    });
    return NULL;
    
found:
    return sub;
}

/* 
 * Note: we cannot use m_mod_is() here as this function may be called
 * from another ctx when M_PS_GLOBAL is set on a broadcast message,
 * and it would fail because it is being called by different thread.
 */
static int tell_if(void *data, const char *key, void *value) {
    m_mod_t *mod = (m_mod_t *)value;
    ps_priv_t *msg = (ps_priv_t *)data;
    ev_src_t *sub = msg->msg.topic ? (ev_src_t *)key : NULL;                 // key is indeed a subscription when we are publishing (check tell_subscribers()) !!

    if (mod->state & (M_MOD_RUNNING | M_MOD_PAUSED) &&                       // mod is running or paused
        ((msg->msg.type != M_PS_USER && msg->msg.sender != mod) ||           // system messages with sender != this module (avoid sending ourselves system messages produced by us)
        (msg->msg.type == M_PS_USER &&                                       // it is a publish and mod is subscribed on topic, or it is a broadcast/direct tell message
        (!msg->msg.topic || sub)))) {
        
        M_DEBUG("Telling a message to '%s'\n", mod->name);
        ps_priv_t *m = alloc_ps_msg(msg, sub);
        if (m) {
            if (write(mod->pubsub_fd[1], &m, sizeof(ps_priv_t *)) != sizeof(ps_priv_t *)) {
                M_DEBUG("Failed to write message: %s\n", strerror(errno));
                m_mem_unref(msg);
            }
        }
    }
    return 0;
}

static ps_priv_t *alloc_ps_msg(const ps_priv_t *msg, ev_src_t *sub) {
    ps_priv_t *m = m_mem_new(sizeof(ps_priv_t), ps_msg_dtor);
    if (m) {        
        memcpy(m, msg, sizeof(ps_priv_t));
        m->msg.sender = m_mem_ref((void *)m->msg.sender); // keep module alive until message is dispatched
        m->sub = m_mem_ref(sub);
    }
    return m;
}

static int tell_global(void *data, const char *key, void *value) {
    m_ctx_t *c = (m_ctx_t *)value;
    ps_priv_t *msg = (ps_priv_t *)data;
    
    return m_map_iterate(c->modules, tell_if, msg);
}

static void ps_msg_dtor(void *data) {
    ps_priv_t *pubsub_msg = (ps_priv_t *)data;
    
    m_mem_unref(pubsub_msg->sub);
    if (pubsub_msg->flags & M_PS_AUTOFREE) {
        memhook._free((void *)pubsub_msg->msg.data);
    }
    if (pubsub_msg->msg.sender) {
        m_mem_unref((void *)pubsub_msg->msg.sender);
    }
}

static inline int get_prio_flag(m_src_flags flag) {
    /* Remove generic src flags (>> 8) and apply PRIO mask */
    return (flag >> 8) & M_SRC_PS_PRIO_MASK;
}

static int tell_subscribers(void *data, const char *key, void *value) {
    m_ctx_t *c = (m_ctx_t *)value;
    ps_priv_t *msg = (ps_priv_t *)data;
    
    m_queue_t *prio_subs[M_SRC_PS_PRIO_SIZE];
    for (int i = 0; i < M_SRC_PS_PRIO_SIZE; i++) {
        prio_subs[i] = m_queue_new(NULL);
    }
    
    m_itr_foreach(c->modules, {
        m_mod_t *mod = m_itr_get(itr);
        ev_src_t *sub = NULL;
        
        if (m_mod_is(mod, M_MOD_RUNNING | M_MOD_PAUSED) && (sub = fetch_sub(mod, msg->msg.topic))) {
            const int idx = __builtin_ctz(get_prio_flag(sub->flags)); // count trailing zeroes
            m_queue_enqueue(prio_subs[idx], sub);
        }
    });
    
    for (int i = M_SRC_PS_PRIO_SIZE - 1; i >= 0; i--) {
        m_itr_foreach(prio_subs[i], {
            ev_src_t *sub = m_itr_get(itr);
            tell_if(msg, (char *)sub, sub->mod);
        });
        m_queue_free(&prio_subs[i]);
    }
}

static int tell_pubsub_msg(ps_priv_t *m, const m_mod_t *recipient, m_ctx_t *c) {    
    if (recipient) { // it is a direct tell
        tell_if(m, NULL, (m_mod_t *)recipient);
    } else {
        /* Broadcast or SYSTEM messages */
        if (m->msg.type != M_PS_USER || !m->msg.topic) {
            m_map_iterate(c->modules, tell_if, m);
        } else {
            tell_subscribers(m, NULL, c);
        }
    }
    return 0;
}

static int send_msg(m_mod_t *mod, const m_mod_t *recipient, const char *topic, 
                    const void *message, m_ps_flags flags) {
    M_PARAM_ASSERT(message);

    mod->stats.sent_msgs++;
    fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
    ps_priv_t m = { { M_PS_USER, mod, topic, message }, flags, NULL };
    return tell_pubsub_msg(&m, recipient, mod->ctx);
}

/** Private API **/

int tell_system_pubsub_msg(const m_mod_t *recipient, m_ctx_t *c, m_ps_types type, m_mod_t *sender, const char *topic) {
    if (sender) {
        // A module sent a M_PS_MOD_POISONPILL message to another
        sender->stats.sent_msgs++;
        fetch_ms(&sender->stats.last_seen, &sender->stats.action_ctr);
    }
    ps_priv_t m = { { type, sender, topic, NULL }, 0, NULL };
    return tell_pubsub_msg(&m, recipient, c);
}

int flush_pubsub_msgs(void *data, const char *key, void *value) {
    m_mod_t *mod = (m_mod_t *)value;
    ps_priv_t *mm = NULL;

    while (read(mod->pubsub_fd[0], &mm, sizeof(ps_priv_t *)) == sizeof(ps_priv_t *)) {
        /*
         * Actually tell msg ONLY if we are not deregistering module,
         * ie: we are stopping looping on the context.
         * Else, just free msg.
         *
         * While stopping module, manage_fds() will call this with data != NULL
         * to let us know we should destroy all enqueued messages.
         */
        if (!data && m_mod_is(mod, M_MOD_RUNNING)) {
            M_DEBUG("Flushing enqueued pubsub message for module '%s'.\n", mod->name);
            m_evt_t *msg = new_evt(M_SRC_TYPE_PS);
            if (msg) {
                msg->ps_evt = &mm->msg;
                run_pubsub_cb(mod, msg, mm->sub);
                continue;
            }
        }
        M_DEBUG("Destroying enqueued pubsub message for module '%s'.\n", mod->name);
        m_mem_unref(mm);
    }
    return 0;
}

void run_pubsub_cb(m_mod_t *mod, m_evt_t *msg, const ev_src_t *src) {
    /* If module is using some different receive function, honor it. */
    m_evt_cb cb = m_stack_peek(mod->recvs);
    if (!cb) {
        /* Fallback to module default receive */
        cb = mod->hook.on_evt;
    }
    
    if (src) {
        /*
         * if src == NULL, do not touch msg->userdata:
         * * it will be NULL when called from ctx loop or flush_ps_messages
         * * it will already be valued when call by m_mod_unstash API
         */
        msg->userdata = src->userptr;
    }
    
    /* Notify underlying fuse fs */
    fs_notify(mod, msg);
        
    /* Finally call user callback */
    cb(mod, msg);

    /* Unref the message */
    m_mem_unref(msg);
    
    mod->stats.recv_msgs++;
    fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
}

/** Public API **/

_public_ int m_mod_ps_subscribe(m_mod_t *mod, const char *topic, m_src_flags flags, const void *userptr) {
    M_MOD_ASSERT_PERM(mod, M_MOD_DENY_SUB);
    M_PARAM_ASSERT(topic);

    /* Check if it is a valid regex: compile it */
    regex_t regex;
    int ret = regcomp(&regex, topic, REG_NOSUB);
    if (ret == 0) {
        M_DEBUG("'%s' is a valid regex.\n", topic);
        
        /* Lazy subscriptions map init */
        if (!mod->subscriptions)  {
            mod->subscriptions = m_map_new(M_MAP_VAL_ALLOW_UPDATE, mem_dtor);
            M_ALLOC_ASSERT(mod->subscriptions);
        } else {
            ev_src_t *old_sub = m_map_get(mod->subscriptions, topic);
            if (old_sub) {
                if (old_sub->flags == flags) {
                    /* Only update userptr */
                    old_sub->userptr = userptr;
                    return 0;
                }
            }
        }

        /* Store new sub as ref'd memory */
        ev_src_t *sub = m_mem_new(sizeof(ev_src_t), subscribtions_dtor);
        M_ALLOC_ASSERT(sub);

        ps_src_t *ps_src = &sub->ps_src;
        sub->type = M_SRC_TYPE_PS;
        sub->flags = flags;
        /*
         * No priority set, default to NORM priority.
         * If multiple priorities are set, the lowest-priority value wins.
         */
        if (get_prio_flag(sub->flags) == 0) {
            sub->flags |= M_SRC_PS_PRIO_NORM;
        }
        sub->userptr = userptr;
        sub->mod = mod;
        memcpy(&ps_src->reg, &regex, sizeof(regex_t));
        ps_src->topic = sub->flags & M_SRC_DUP ? mem_strdup(topic) : topic;
        ret = m_map_put(mod->subscriptions, ps_src->topic, sub); // M_MAP_VAL_ALLOW_UPDATE -> this will dtor old elem before updating
        if (ret == 0) {
            fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
        }
    }
    return ret;
}

_public_ int m_mod_ps_unsubscribe(m_mod_t *mod, const char *topic) {
    M_MOD_ASSERT_PERM(mod, M_MOD_DENY_SUB);
    M_PARAM_ASSERT(topic);
    
    int ret = m_map_remove(mod->subscriptions, topic);
    if (ret == 0) {
        fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
        if (m_map_len(mod->subscriptions) == 0) {
            m_map_free(&mod->subscriptions);
        }
    }
    return ret;
}

_public_ int m_mod_ps_tell(m_mod_t *mod, const m_mod_t *recipient, const void *message, m_ps_flags flags) {
    M_MOD_ASSERT_PERM(mod, M_MOD_DENY_PUB);
    M_PARAM_ASSERT(recipient);
    /* only same ctx modules can talk */
    M_PARAM_ASSERT(mod->ctx == recipient->ctx);

    /* Eventually cleanup PS_GLOBAL flag */    
    return send_msg(mod, recipient, NULL, message, flags);
}

_public_ int m_mod_ps_publish(m_mod_t *mod, const char *topic, const void *message, m_ps_flags flags) {
    M_MOD_ASSERT_PERM(mod, M_MOD_DENY_PUB);
    M_PARAM_ASSERT(topic);
    
    /* Eventually cleanup PS_GLOBAL flag */    
    return send_msg(mod, NULL, topic, message, flags);
}

_public_ int m_mod_ps_broadcast(m_mod_t *mod, const void *message, m_ps_flags flags) {
    M_MOD_ASSERT_PERM(mod, M_MOD_DENY_PUB);
    M_PARAM_ASSERT(message);
    
    return send_msg(mod, NULL, NULL, message, flags);
}

_public_ int m_mod_ps_poisonpill(m_mod_t *mod, const m_mod_t *recipient) {
    M_MOD_ASSERT_PERM(mod, M_MOD_DENY_PUB);
    M_PARAM_ASSERT(recipient);
    /* only same ctx modules can talk */
    M_PARAM_ASSERT(mod->ctx == recipient->ctx);
    M_PARAM_ASSERT(m_mod_is(recipient, M_MOD_RUNNING));

    return tell_system_pubsub_msg(recipient, mod->ctx, M_PS_MOD_POISONPILL, mod, NULL);
}