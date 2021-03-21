#include <module/mod.h>
#include <module/ctx.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <module/mem.h>

static bool A_init(m_mod_t *mod);
static bool B_init(m_mod_t *mod);
static void A_recv(m_mod_t *mod, const m_evt_t *msg);
static void A_recv_ready(m_mod_t *mod, const m_evt_t *msg);
static void B_recv(m_mod_t *mod, const m_evt_t *msg);
static void B_recv_sleeping(m_mod_t *mod, const m_evt_t *msg);
static void A_dtor(m_mod_t *mod);

static m_mod_t *refB;

/*
 * Create "A" and "B" modules in ctx_name context.
 * These modules can share some callbacks.
 */
void create_modules(m_ctx_t *c, m_mod_t **modA, m_mod_t **modB) {
    m_mod_hook_t hookA = (m_mod_hook_t) {A_init, NULL, A_recv, A_dtor };
    m_mod_hook_t hookB = (m_mod_hook_t) {B_init, NULL, B_recv, NULL };
    
    m_mod_register("Pippo", c, modA, &hookA, 0, NULL);
    m_mod_register("Doggo", c, modB, &hookB, 0, NULL);
}

static bool A_init(m_mod_t *mod) {
    refB = m_mod_ref(mod, "Doggo");
    m_mod_src_register(mod, STDIN_FILENO, 0, NULL);
    return true;
}

static bool B_init(m_mod_t *mod) {
    /* Doggo should subscribe to "leaving" topic */
    m_mod_src_register(mod, "leaving", 0, NULL);
    return true;
}

static void A_dtor(m_mod_t *mod) {
    m_mem_unref(refB);
}

/*
 * Our A module's poll callback.
 */
static void A_recv(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type != M_SRC_TYPE_PS) {
        char c;
        (void)!read(msg->fd_evt->fd, &c, sizeof(char));
        
        switch (tolower(c)) {
            case 'c':
                m_mod_log(mod, "Doggo, come here!\n");
                m_mod_ps_tell(mod, refB, (unsigned char *)"ComeHere", 0);
                break;
            case 'q':
                m_mod_log(mod, "I have to go now!\n");
                m_mod_ps_publish(mod, "leaving", (unsigned char *)"ByeBye", 0);
                m_ctx_quit(m_mod_ctx(mod), 0);
                break;
            default:
                /* Avoid newline */
                if (c != 10) {
                    m_mod_log(mod, "You got to call your doggo first. Press 'c'.\n");
                }
                break;
        }
    } else {
        if (msg->ps_evt->type == M_PS_USER && !strcmp((char *)msg->ps_evt->data, "BauBau")) {
            m_mod_become(mod, A_recv_ready);
            m_mod_log(mod, "Press 'p' to play with Doggo! Or 'f' to feed your Doggo. 's' to have a nap. 'w' to wake him up. 'q' to leave him for now.\n");
        }
    }
}

static void A_recv_ready(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type != M_SRC_TYPE_PS) {
        char c;
        (void)!read(msg->fd_evt->fd, &c, sizeof(char));
        
        switch (tolower(c)) {
            case 'p':
                m_mod_log(mod, "Doggo, let's play a bit!\n");
                m_mod_ps_tell(mod, refB, (unsigned char *)"LetsPlay", 0);
                break;
            case 's':
                m_mod_log(mod, "Doggo, you should sleep a bit!\n");
                m_mod_ps_tell(mod, refB, (unsigned char *)"LetsSleep", 0);
                break;
            case 'f':
                m_mod_log(mod, "Doggo, you want some of these?\n");
                m_mod_ps_tell(mod, refB, (unsigned char *)"LetsEat", 0);
                break;
            case 'w':
                m_mod_log(mod, "Doggo, wake up!\n");
                m_mod_ps_tell(mod, refB, (unsigned char *)"WakeUp", 0);
                break;
            case 'q':
                m_mod_log(mod, "I have to go now!\n");
                m_mod_ps_publish(mod, "leaving", (unsigned char *)"ByeBye", 0);
                m_ctx_quit(m_mod_ctx(mod), 0);
                break;
            default:
                /* Avoid newline */
                if (c != 10) {
                    m_mod_log(mod, "Unrecognized command. Beep. Please enter a new one... Totally not a bot.\n");
                }
                break;
        }
    }
}

/*
 * Our B module's poll callback.
 */
static void B_recv(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type == M_SRC_TYPE_PS) {
        switch (msg->ps_evt->type) {
            case M_PS_USER:
                if (!strcmp((char *)msg->ps_evt->data, "ComeHere")) {
                    m_mod_log(mod, "Running...\n");
                    m_mod_ps_tell(mod, msg->ps_evt->sender, (unsigned char *)"BauBau", 0);
                } else if (!strcmp((char *)msg->ps_evt->data, "LetsPlay")) {
                    m_mod_log(mod, "BauBau BauuBauuu!\n");
                } else if (!strcmp((char *)msg->ps_evt->data, "LetsEat")) {
                    m_mod_log(mod, "Burp!\n");
                } else if (!strcmp((char *)msg->ps_evt->data, "LetsSleep")) {
                    m_mod_become(mod, B_recv_sleeping);
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

static void B_recv_sleeping(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type == M_SRC_TYPE_PS && msg->ps_evt->type == M_PS_USER) {
        if (!strcmp((char *)msg->ps_evt->data, "WakeUp")) {
            m_mod_unbecome(mod);
            m_mod_log(mod, "Yawn...\n");
        } else {
            m_mod_log(mod, "ZzzZzz...\n");
        }
    }
}
