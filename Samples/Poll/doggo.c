#include <module/mod_easy.h> 
#include <string.h>

static void m_mod_on_evt_sleeping(m_mod_t *mod, const m_evt_t *msg);

M_MOD("Doggo");

static void m_mod_pre_start(void) {

}

static bool m_mod_on_start(m_mod_t *mod) {
    /* Doggo should subscribe to "leaving" topic */
    m_mod_src_register(mod, "leaving", 0, NULL);
    return true;
}

static bool m_mod_on_eval(m_mod_t *mod) {
    return true;
}

static void m_mod_on_stop(m_mod_t *mod) {
    
}

static void m_mod_on_evt(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type == M_SRC_TYPE_PS) {
        switch (msg->ps_evt->type) {
        case M_PS_USER:
            if (!strcmp((char *)msg->ps_evt->data, "ComeHere")) {
                m_mod_log(mod, "Running...\n");
                m_mod_ps_tell(mod, msg->ps_evt->sender, "BauBau", 0);
            } else if (!strcmp((char *)msg->ps_evt->data, "LetsPlay")) {
                m_mod_log(mod, "BauBau BauuBauuu!\n");
            } else if (!strcmp((char *)msg->ps_evt->data, "LetsEat")) {
                m_mod_log(mod, "Burp!\n");
            } else if (!strcmp((char *)msg->ps_evt->data, "LetsSleep")) {
                m_mod_become(mod, m_mod_on_evt_sleeping);
                m_mod_log(mod, "ZzzZzz...\n");
            } else if (!strcmp((char *)msg->ps_evt->data, "ByeBye")) {
                m_mod_log(mod, "Sob...\n");
            } else if (!strcmp((char *)msg->ps_evt->data, "WakeUp")) {
                m_mod_log(mod, "???\n");
            }
            break;
        default:
            break;
        }
    }
}

static void m_mod_on_evt_sleeping(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type == M_SRC_TYPE_PS && msg->ps_evt->type == M_PS_USER) {
        if (!strcmp((char *)msg->ps_evt->data, "WakeUp")) {
            m_mod_unbecome(mod);
            m_mod_log(mod, "Yawn...\n");
        } else {
            m_mod_log(mod, "ZzzZzz...\n");
        }
    }
}
