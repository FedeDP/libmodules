#include <assert.h>
#include "ctx.h"
#include "priv.h"

/***********************************************************
 * Code related to main library ctor/dtor + main() symbol. *
 ***********************************************************/

m_map_t *ctx = NULL;
m_memhook_t memhook = { malloc, calloc, free };
pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;

_public_ _m_ctor0_ _weak_ void m_pre_start(void) {
    M_DEBUG("Pre-starting libmodule.\n");
}

_public_ _weak_ void m_ctx_pre_loop(m_ctx_t *c, int argc, char *argv[]) {
    M_DEBUG("Pre-looping libmodule easy API.");
}

/*
 * This is an exported global weak symbol.
 * It means that if a program does not implement any main(int, char *[]),
 * this will be used by default.
 *
 * All it does is looping on default ctx.
 */
_public_ _weak_ int main(int argc, char *argv[]) {
    m_ctx_t *c = m_map_peek(ctx);
    m_ctx_pre_loop(c, argc, argv);
    return m_ctx_loop(c);
}

static _m_ctor1_ void libmodule_init(void) {
    M_DEBUG("Initializing libmodule %d.%d.%d.\n", LIBMODULE_VERSION_MAJ, LIBMODULE_VERSION_MIN, LIBMODULE_VERSION_PAT);
    ctx = m_map_new(0, mem_dtor);
    assert(ctx != NULL);
    pthread_mutex_init(&mx, NULL);
}

static _m_dtor0_ void libmodule_deinit(void) {
    M_DEBUG("Destroying libmodule.\n");
    m_map_free(&ctx);
    pthread_mutex_destroy(&mx);
}

/** Public API **/

int m_set_memhook(const m_memhook_t *hook) {
    /*
     * Check that we are called from within m_pre_start,
     * when library is not yet initialized
     */
    M_RET_ASSERT(!ctx, -EPERM);
    M_PARAM_ASSERT(hook);
    M_PARAM_ASSERT(hook->_malloc);
    M_PARAM_ASSERT(hook->_calloc);
    M_PARAM_ASSERT(hook->_free);

    memcpy(&memhook, hook, sizeof(m_memhook_t));
    return 0;
}
