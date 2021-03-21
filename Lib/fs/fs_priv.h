#include "poll_priv.h"

#define dummy(x) x { return -ENOSYS; }

#ifndef WITH_FS
#define FsExposed(x) dummy(x)
#else
#define FsExposed(x) x
#endif

FsExposed(int fs_init(m_ctx_t *c));
FsExposed(int fs_process(m_ctx_t *c));
FsExposed(int fs_notify(m_mod_t *mod, const m_evt_t *msg));
FsExposed(int fs_cleanup(m_mod_t *mod));
FsExposed(int fs_end(m_ctx_t *c));
