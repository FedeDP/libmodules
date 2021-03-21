#include <module/ctx.h>
#include <module/mod.h>

extern void create_modules(m_ctx_t *c, m_mod_t **modA, m_mod_t **modB);

/*
 * This function is automatically called before initing any module.
 * Use this function to eg: parse config needed to decide
 * whether to start some module.
 * There is no need to explicitly call it.
 */
void m_pre_start() {
    printf("Press 'c' to start playing with your own doggo...\n");
}

int main(int argc, char *argv[]) {
    /*
     * Register the context
     */
    m_ctx_t *c = NULL;
    m_ctx_register("SharedSrc", &c, 0, NULL);

    /*
     * Create the modules in the context
     */
    m_mod_t *modA = NULL, *modB = NULL;
    create_modules(c, &modA, &modB);

    /*
     * Loop on context to fetch modules' events
     */
    m_ctx_loop(c);
    
    /*
     * Finally, destroy our modules.
     * As context had no M_CTX_PERSIST flag,
     * it will be automatically destroyed when it has no module in it.
     */
    m_mod_deregister(&modA);
    m_mod_deregister(&modB);
    return 0;
}
