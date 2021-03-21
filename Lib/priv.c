#include "priv.h"

/*******************************************
 * Code related to generic priv utilities. *
 *******************************************/

static void evt_dtor(void *data) {
    m_evt_t *evt = (m_evt_t *)data;
    /* We use fd_evt as all messages share address inside union */
    m_mem_unref(evt->fd_evt);
}

/** Private API **/

char *mem_strdup(const char *s) {
    char *new = NULL;
    if (s) {
        const size_t len = strlen(s) + 1;
        new = memhook._malloc(len);
        if (new) {
            memcpy(new, s, len);
        }
    }
    return new;
}

void fetch_ms(uint64_t *val, uint64_t *ctr) {
    struct timespec spec;
#ifdef __linux__
    clock_gettime(CLOCK_BOOTTIME, &spec);
#else
    clock_gettime(CLOCK_MONOTONIC, &spec);
#endif
    *val = spec.tv_sec * 1000 + spec.tv_nsec / 1000000;
    
    if (ctr) {
        (*ctr)++;
    }
}

m_evt_t *new_evt(m_src_types type) {
    m_evt_t *msg = m_mem_new(sizeof(m_evt_t), evt_dtor);
    if (msg) {
        msg->type = type;
    }
    return msg;
}
