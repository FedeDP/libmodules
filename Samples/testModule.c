#include <module/mod_easy.h>
#include <string.h>

/** This is a test module needed for Easy example, to runtime link another module **/

M_MOD("Test");

/*
 * This function is automatically called before registering the module. 
 * Use this to set some  global state needed eg: in check() function 
 */
static void m_mod_pre_start(void) {
}

/*
 * Initializes this module's state;
 * returns a valid fd to be polled.
 */
static bool m_mod_on_start(m_mod_t *mod) {
    m_mod_src_register(mod, "leaving", 0, NULL);
    m_mod_log(mod, "Linked.\n");
    return true;
}

/* 
 * Should return not-0 value when module can be actually started (and thus polled).
 * Use this to check intra-modules dependencies or any other env variable.
 * 
 * Eg: you can evaluate your global state to make this module start right after
 * certain conditions are met.
 */
static bool m_mod_on_eval(m_mod_t *mod) {
    return true;
}

/*
 * Destroyer function, called at module unload (at end of program).
 * Note that any module's fds are automatically closed for you.
 */
static void m_mod_on_stop(m_mod_t *mod) {

}

/*
 * Default poll callback
 */
static void m_mod_on_evt(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type == M_SRC_TYPE_PS && msg->ps_evt->type == M_PS_USER) {
        if (!strcmp((char *)msg->ps_evt->data, "ByeBye")) {
            m_mod_log(mod, "Received quit.\n");
        }
    }
}
