#include "fs_priv.h"

#define FUSE_USE_VERSION 35

#include "mod.h"
#include "mem.h"
#include "fs.h"
#include <fuse.h>
#include <fuse_lowlevel.h>  // to get fuse fd to process events internally
#include <sys/poll.h>       // poll operation support

#define FS_PRIV()         fs_ctx_t *f = (fs_ctx_t *)c->fs; if (!f) { return -EINVAL; }
#define FS_CTX()          m_ctx_t *c = fuse_get_context()->private_data;
#define FS_CLIENT()       fs_client_t *cl = (fs_client_t *)fi->fh; if (!cl) { return -ENOENT; }
#define FS_MOD()          FS_CLIENT(); m_mod_t *mod = cl->mod;

/* Context fs priv data */
typedef struct {
    struct fuse *handler;
    struct fuse_buf buf;
    struct fuse_args args;
    time_t start;
    ev_src_t *src;
} fs_ctx_t;

/* poll client fs data */
typedef struct {
    m_mod_t *mod;
    struct fuse_pollhandle *ph;
    int in_evt;
    char *read_buf;
    size_t read_len;
    size_t write_len;
    bool read_all;
} fs_client_t;

/* Module fs priv data (list of clients) */
typedef struct {
    m_list_t *clients;
} fs_priv_t;

/* FS-related functions */
static int fs_getattr(const char *path, struct stat *stbuf,
                      struct fuse_file_info *fi);
static int fs_open(const char *path, struct fuse_file_info *fi);
static int fs_release(const char *path, struct fuse_file_info *fi);
static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                      off_t offset, struct fuse_file_info *fi,
                      enum fuse_readdir_flags flags);

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int fs_unlink(const char *path);
static int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi);
static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
static int fs_poll(const char *path, struct fuse_file_info *fi,
                   struct fuse_pollhandle *ph, unsigned *reventsp);
static int fs_ioctl(const char *path, unsigned int cmd, void *arg,
                    struct fuse_file_info *fi, unsigned int flags, void *data);

/* Internal functions */
static void client_dtor(void *data);
static void fs_logger(const m_mod_t *mod, const char *fmt, va_list args);
static void receive(m_mod_t *self, const m_evt_t *const msg);
static void fs_wakeup_clients(fs_priv_t *fp, bool leaving);

static const struct fuse_operations operations = {
    .getattr    = fs_getattr,
    .open       = fs_open,
    .release    = fs_release,
    .readdir    = fs_readdir,
    .read       = fs_read,
    .unlink     = fs_unlink,
    .create     = fs_create,
    .utimens    = fs_utimens,
    .poll       = fs_poll,
    .ioctl      = fs_ioctl
};

/** Fuse OPS Start **/
static int fs_getattr(const char *path, struct stat *stbuf,
                         struct fuse_file_info *fi) {
    FS_CTX();
    FS_PRIV();
    
    memset(stbuf, 0, sizeof(struct stat));
    
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();
    stbuf->st_atime = f->start;
    stbuf->st_mtime = f->start;
    if (strcmp(path, "/") == 0) { // root dir of fuse fs
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = m_map_len(c->modules);
        return 0;
    } 
    if (strlen(path) > 1 && m_map_contains(c->modules, path + 1)) {
        stbuf->st_mode = S_IFREG | 0444;
        stbuf->st_nlink = 1;
        stbuf->st_size = 1024; // non-zero size
        return 0;
    }
    return -ENOENT;
}

static int fs_open(const char *path, struct fuse_file_info *fi) {
    FS_CTX();
    
    if (strlen(path) > 1) {
        m_mod_t *mod = m_map_get(c->modules, path + 1);
        if (mod) {
            fs_client_t *cl = memhook._calloc(1, sizeof(fs_client_t));
            M_ALLOC_ASSERT(cl);

            if (!mod->fs) {
                mod->fs = memhook._calloc(1, sizeof(fs_priv_t));
                if (!mod->fs) {
                    memhook._free(cl);
                    return -ENOMEM;
                }
            }
            fs_priv_t *fp = (fs_priv_t *)mod->fs;
            if (!fp->clients) { 
                fp->clients = m_list_new(NULL, client_dtor);
                if (!fp->clients) {
                    memhook._free(cl);
                    memhook._free(fp);
                    mod->fs = NULL;
                    return -ENOMEM;
                }
            }
            cl->mod = m_mem_ref(mod); // keep module alive while any client uses it
            m_list_insert(fp->clients, cl);
            fi->fh = (uint64_t) cl;
            fi->direct_io = 1;
            fi->nonseekable = 1;
            return 0;
        }
        return -ENOENT;
    }
    return -EISDIR;
}

static int fs_release(const char *path, struct fuse_file_info *fi) {
    FS_MOD();
    fi->fh = 0;
    fs_priv_t *fp = (fs_priv_t *)mod->fs;
    if (fp) {
        m_list_remove(fp->clients, cl);
        if (m_list_len(fp->clients) == 0) {
            m_list_free(&fp->clients);
            memhook._free(fp);
            mod->fs = NULL;
        }
        return 0;
    }
    return -ENOENT;
}

static int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    if (strcmp(path, "/")) {
        return -ENOENT;
    }
        
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);
    
    FS_CTX();
    m_itr_foreach(c->modules, {
        m_mod_t *mod = m_itr_get(itr);
        filler(buf, mod->name, NULL, 0, 0);
    });
    return 0;
}

/* NOTE: partial reads are unsupported!! */
static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {    
    FS_CTX();
    FS_MOD();
        
    cl->read_buf = buf;
    cl->read_len = size;
    cl->write_len = 0;
    errno = 0;
    
    if (!cl->read_all) {
        /* Store old context logger and replace it with a fuse one */
        m_log_cb old_log = c->logger;
        const void *old_userdata = mod->userdata;
        c->logger = fs_logger;
        mod->userdata = (void *)fi->fh;
    
        m_mod_dump(mod);
        if (cl->write_len > cl->read_len) {
            cl->write_len = cl->read_len;
        }
            
        /* Restore real context logger */
        c->logger = old_log;
        mod->userdata = old_userdata;
        cl->read_all = true;
    } else {
        cl->read_all = false;
    }

    cl->read_buf = NULL;
    cl->read_len = 0;
    return cl->write_len;
}

static int fs_unlink(const char *path) {
    FS_CTX();
    if (strlen(path) > 1) {
        m_mod_t *mod = m_map_get(c->modules, path + 1);
        if (mod) {
            if (m_mod_deregister(&mod) == 0) {
                return 0;
            }
            return -EPERM;
        }
    }
    return -ENOENT;
}

static int fs_utimens(const char *path, const struct timespec tv[2], struct fuse_file_info *fi) {
    return 0;
}

static int fs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    const m_mod_hook_t fuse_hook = {.on_evt = receive };
    if (strlen(path) > 1) {
        FS_CTX();
        m_mod_t *mod = NULL;
        if (m_mod_register(path + 1, c, &mod, &fuse_hook, M_MOD_NAME_DUP, NULL) == 0) {
            return 0;
        }
        return -EPERM;
    }
    return -ENOENT;
}

static int fs_poll(const char *path, struct fuse_file_info *fi,
                     struct fuse_pollhandle *ph, unsigned *reventsp) {
    FS_CLIENT();

    if (ph) {
        if (cl->in_evt) {
            if (cl->in_evt == 1) {
                *reventsp |= POLLIN;
            } else {
                // Module has been deregistered
                *reventsp |= POLLERR;
            }
            fuse_pollhandle_destroy(ph);
            cl->in_evt = 0;
        } else {
            cl->ph = ph; // store pollhandle for future notifications
        }
    }
    return 0;
}

static int fs_ioctl(const char *path, unsigned int cmd, void *arg,
                      struct fuse_file_info *fi, unsigned int flags, void *data) {
    FS_MOD();
    
    if (flags & FUSE_IOCTL_COMPAT) {
        return -ENOSYS;
    }
    
    if (flags & FUSE_IOCTL_DIR) {
        return -EINVAL;
    }
    
    switch (cmd) {
        case MOD_STATE:
            *(m_mod_states *)data = mod->state;
            return 0;
        case MOD_START:
            return m_mod_start(mod);
        case MOD_STOP:
            return m_mod_stop(mod);
        case MOD_RESUME:
            return m_mod_resume(mod);
        case MOD_PAUSE:
            return m_mod_pause(mod);
        case MOD_STATS:
            return m_mod_stats(mod, data);
        default:
            break;
    }
    return -EINVAL;
}
/** Fuse OPS End **/

static void client_dtor(void *data) {
    fs_client_t *cl = (fs_client_t *)data;
    if (cl->ph) {
        fuse_pollhandle_destroy(cl->ph);
        cl->ph = NULL;
    }
    m_mem_unref(cl->mod);
    memhook._free(cl);
}

static void fs_logger(const m_mod_t *mod, const char *fmt, va_list args) {
    fs_client_t *cl = (fs_client_t *)mod->userdata;
    if (cl->write_len < cl->read_len) {
        /* 
         * vsnprintf: If the output was truncated due to this limit then the return value 
         * is the number of characters (excluding the terminating null byte) which would have been written 
         * to the final string if enough space had been available.
         * 
         * After module_dump() call, f->write_len is enforced to be at maximum f->read_len.
         */
        cl->write_len += vsnprintf(cl->read_buf + cl->write_len, cl->read_len - cl->write_len, fmt, args);
    }
}

static void receive(m_mod_t *self, const m_evt_t *const msg) {
    
}

static void fs_wakeup_clients(fs_priv_t *fp, bool leaving) {
    m_itr_foreach(fp->clients, {
        fs_client_t *cl = m_itr_get(itr);
        if (cl->ph) {
            cl->in_evt = leaving ? -1 : 1;
            fuse_notify_poll(cl->ph);
            fuse_pollhandle_destroy(cl->ph);
            cl->ph = NULL;
        }
    });
}

/** Private API **/

int fs_init(m_ctx_t *c) {
    int ret = mkdir(c->fs_root, 0777);
    if (ret != 0) {
        return -errno;
    }

    c->fs = memhook._calloc(1, sizeof(fs_ctx_t));
    M_ALLOC_ASSERT(c->fs);
        
    FS_PRIV();

    /* Mandatory fuse arg: app name */
    fuse_opt_add_arg(&f->args, "libmodule");
        
    f->handler = fuse_new(&f->args, &operations, sizeof(operations), c);
    M_ALLOC_ASSERT(f->handler);
        
    f->start = time(NULL);
        
    ret = fuse_mount(f->handler, c->fs_root);
    if (ret == 0) {
        f->src = memhook._calloc(1, sizeof(ev_src_t));
        M_ALLOC_ASSERT(f->src);
            
        /* Actually register fuse fd in poll plugin */
        f->src->type = M_SRC_TYPE_FD;
        f->src->fd_src.fd = fuse_session_fd(fuse_get_session(f->handler));
        ret = poll_set_new_evt(&c->ppriv, f->src, ADD);
    }
    return ret;
}

int fs_process(m_ctx_t *c) {
    FS_PRIV();

    struct fuse_session *sess = fuse_get_session(f->handler);
    int ret = fuse_session_receive_buf(sess, &f->buf);
    if (ret > 0) {
        fuse_session_process_buf(sess, &f->buf);
        return 0;
    }
    M_DEBUG("Fuse: failed to retrieve buffer.\n");
    return ret;
}

int fs_notify(m_mod_t *mod, const m_evt_t *msg) {
    if (mod && mod->fs) {
        fs_priv_t *fp = (fs_priv_t *)mod->fs;
        if (msg->type == M_SRC_TYPE_PS && msg->ps_evt->type == M_PS_CTX_STOPPED) {
            /* When loop gets stopped, destroy clients list */
            m_list_free(&fp->clients);
            memhook._free(fp);
            mod->fs = NULL;
        } else {
            fs_wakeup_clients(fp, false);
        }
    }
    return 0;
}

int fs_cleanup(m_mod_t *mod) {
    if (mod->fs) {
        fs_priv_t *fp = (fs_priv_t *)mod->fs;
        fs_wakeup_clients(fp, true);
    }
    return 0;
}

int fs_end(m_ctx_t *c) {
    FS_PRIV();
    
    /* Deregister fuse fd */
    poll_set_new_evt(&c->ppriv, f->src, RM);
    memhook._free(f->src);

    /* Free fuse recv buf */
    memhook._free(f->buf.mem);
    
    /* Destroy fuse handler */
    fuse_unmount(f->handler);
    fuse_destroy(f->handler);
    
    /* Free fuse args */
    fuse_opt_free_args(&f->args);

    /* Delete folder */
    remove(c->fs_root);
    
    memhook._free(c->fs);
    c->fs = NULL;
    return 0;
}

/** Public API **/

_public_ const char *m_ctx_fs_get_root(const m_ctx_t *c) {
    M_RET_ASSERT(c, NULL);
    M_RET_ASSERT(c->state != M_CTX_ZOMBIE, NULL);

    return c->fs_root;
}

_public_ int m_ctx_fs_set_root(m_ctx_t *c, const char *path) {
    M_CTX_ASSERT(c);
    M_RET_ASSERT(c->state == M_CTX_IDLE, -EPERM);
    M_PARAM_ASSERT(path && strlen(path));

    if (c->fs_root) {
        memhook._free(c->fs_root);
    }
    c->fs_root = mem_strdup(path);
    return 0;
}
