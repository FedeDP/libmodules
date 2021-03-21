#include <module/mod_easy.h>
#include <module/ctx.h>

M_MOD("Pippo");

static int thData;
static int tmrData;

static int inc(void *udata);

/* Hook on m_ctx_pre_loop() weak symbol */
void m_ctx_pre_loop(m_ctx_t *c, int argc, char *argv[]) {
    printf("Task example starting.\n");
}

static void m_mod_pre_start(void) {

}

static bool m_mod_on_start(m_mod_t *mod) {
    m_mod_log(mod,"Starting data val: %d\n", thData);
    
    m_mod_src_register(mod, &((m_src_tmr_t) { CLOCK_MONOTONIC, 1000 }), 0, &tmrData);
    m_mod_src_register(mod, &((m_src_task_t) { 8, inc }), 0, &thData);
    return true;
}

static bool m_mod_on_eval(m_mod_t *mod) {
    return true;
}

static void m_mod_on_stop(m_mod_t *mod) {
    
}

static void m_mod_on_evt(m_mod_t *mod, const m_evt_t *msg) {
    switch (msg->type) {
    case M_SRC_TYPE_TMR: {
        int *data = (int *)msg->userdata;
        if (*data == 5) {
            m_mod_log(mod, "Timed out.\n");
            m_ctx_quit(m_mod_ctx(mod), 0);
            m_mod_log(mod,"Final data val: %d\n", thData);
        } else {
            (*data)++;
            m_mod_log(mod, "Clock... %d\n", *data);
        }
        break;
    }
    case M_SRC_TYPE_TASK:
        m_mod_log(mod,"Task id: %u ended with retval: %d\n", msg->task_evt->tid, msg->task_evt->retval);
        break;
    default:
        break;
    }
}

static int inc(void *udata) {
    int *d = (int *)udata;
    while (*d < 3) {
        (*d)++;
        sleep(1);
    }
    return 0;
}
