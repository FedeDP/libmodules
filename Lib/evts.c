#include "priv.h"

/************************************
 * Code related to events handling. *
 ************************************/

/* Must be unref through m_mem_unref() */
_public_ m_mod_t *m_mod_ref(const m_mod_t *mod, const char *name) {
    M_RET_ASSERT(mod, NULL);
    M_RET_ASSERT(name, NULL);
    M_MOD_CTX(mod);

    m_mod_t *m = m_map_get(c->modules, name);
    return m_mem_ref(m);
}

_public_ int m_mod_become(m_mod_t *mod, m_evt_cb new_on_evt) {
    M_PARAM_ASSERT(new_on_evt);
    M_MOD_ASSERT_STATE(mod, M_MOD_RUNNING);

    int ret = m_stack_push(mod->recvs, new_on_evt);
    if (ret == 0) {
        fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
    }
    return ret;
}

_public_ int m_mod_unbecome(m_mod_t *mod) {
    M_MOD_ASSERT_STATE(mod, M_MOD_RUNNING);

    if (m_stack_pop(mod->recvs) != NULL) {
        fetch_ms(&mod->stats.last_seen, &mod->stats.action_ctr);
        return 0;
    }
    return -EINVAL;
}

_public_ int m_mod_stash(m_mod_t *mod, const m_evt_t *evt) {
    M_MOD_ASSERT(mod);
    M_PARAM_ASSERT(evt);
    // Cannot stash FD evts: it would cause an infinite loop polling on it
    M_PARAM_ASSERT(evt->type != M_SRC_TYPE_FD);

    m_mem_ref((void *)evt);
    return m_queue_enqueue(mod->stashed, (void *)evt);
}

_public_ int m_mod_unstash(m_mod_t *mod) {
    M_MOD_ASSERT(mod);

    m_evt_t *evt = m_queue_dequeue(mod->stashed);
    if (evt) {
        run_pubsub_cb(mod, evt, NULL);
    }
    return 0;
}

_public_ int m_mod_unstashall(m_mod_t *mod) {
    M_MOD_ASSERT(mod);

    m_itr_foreach(mod->stashed, {
            m_evt_t *evt = m_itr_get(itr);
            /*
             * Here evt has 1 ref; run_pubsub_cb() would drop it
             * thus invalidating ptr.
             * But m_queue_clear() still needs ptrs!
             * Keep evts alive.
             */
            m_mem_ref(evt);
            run_pubsub_cb(mod, evt, NULL);
    });
    /* Return number of events unstashed or error */
    const int len = m_queue_len(mod->stashed);
    int ret = m_queue_clear(mod->stashed);
    if (ret == 0) {
        ret = len;
    }
    return ret;
}