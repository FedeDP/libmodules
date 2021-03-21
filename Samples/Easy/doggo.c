#include <module/mod_easy.h>
#include <module/ctx.h> 
#include <string.h>

static void m_mod_on_evt_sleeping(m_mod_t *mod, const m_evt_t *msg);

static const m_mod_t *new_mod;

M_MOD("Doggo");

static void m_mod_pre_start(void) {
    printf("Press 'c' to start playing with your own doggo...\n");
}

static bool m_mod_on_start(m_mod_t *mod) {
    /* Doggo should subscribe to "leaving" topic, as regex */
    m_mod_src_register(mod, "leav[i+]ng", 0, NULL);
    return true;
}

static bool m_mod_on_eval(m_mod_t *mod) {
    return true;
}

static void m_mod_on_stop(m_mod_t *mod) {
    
}

/*
 * Default poll callback
 */
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
                
                /* Test runtime module loading; loaded module won't have direct access to CTX */
                m_mod_load(mod, "./libtestmod.so", M_MOD_DENY_CTX, NULL);
            } else if (!strcmp((char *)msg->ps_evt->data, "ByeBye")) {
                m_mod_log(mod, "Sob...\n");
            } else if (!strcmp((char *)msg->ps_evt->data, "WakeUp")) {
                m_mod_log(mod, "???\n");
            }
            break;
        case M_PS_MOD_STOPPED: {
                if (msg->ps_evt->sender) {
                    const char *name = m_mod_name(msg->ps_evt->sender);
                    m_mod_log(mod, "Module '%s' has been stopped.\n", name);
                } else {
                    m_mod_log(mod, "A module has been deregistered.\n");
                }
            }
            break;
        default:
            break;
        }
    }
}

static void m_mod_on_evt_sleeping(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type == M_SRC_TYPE_PS) {
        if (msg->ps_evt->type == M_PS_USER) {
            if (!strcmp((char *)msg->ps_evt->data, "WakeUp")) {
                m_mod_unbecome(mod);
                m_mod_log(mod, "Yawn...\n");
                m_mod_ps_poisonpill(mod, new_mod);
            } else {
                m_mod_log(mod, "ZzzZzz...\n");
            }
        } else if (msg->ps_evt->type == M_PS_MOD_STARTED) {
            new_mod = msg->ps_evt->sender;
            /* A new module has been started */
            const char *name = m_mod_name(new_mod);
            m_mod_log(mod, "Module '%s' has been started.\n", name);
        }
    }
}
