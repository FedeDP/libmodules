/*
 * Based upon work from Mathias Brossard <mathias@brossard.org>.
 * See: https://github.com/mbrossard/threadpool
 * 
 * Thank you!
 */

#include "priv.h"
#include <stdatomic.h>

#define M_THREADS_ASSERT(pool, ret) \
    M_RET_ASSERT(pool->shutdown == SHUTDOWN_NO, -EPERM); \
    M_RET_ASSERT(pool->init_state & INITED_STARTED, -EPERM);  

typedef enum {
    INITED_THREADS  = 0x01,     // threads are allocated
    INITED_TASKS    = 0x02,     // tasks queue is allocated
    INITED_MUT      = 0x04,     // mutex has been inited
    INITED_COND     = 0x08,     // condition variable has been inited
    INITED_STARTED  = 0x10,     // threads have been actually started
} thpool_inited_t;

typedef enum {
    SHUTDOWN_NO,
    SHUTDOWN_WAITCURR,
    SHUTDOWN_WAITALL,
} thpool_shutdown_t;

typedef struct {
    m_thpool_task fn;
    void *arg;
} thpool_task_t;

struct _thpool {
    uint8_t max_threads;            /* Nobody writes this but us during thpool_new. No need to use an atomic */
    thpool_inited_t init_state;     /* Nobody writes this but us during thpool_new. No need to use an atomic */
    thpool_shutdown_t shutdown;     /* Always used behind a mutex */
    pthread_mutex_t lock;
    pthread_cond_t notify;
    m_list_t *threads;              /* Always used behind a mutex */
    m_queue_t *tasks;               /* Always used behind a mutex */
    atomic_uint running_tasks;
    m_thpool_flags flags;           /* Nobody writes this but us during thpool_new. No need to use an atomic */
};

static void *thpool_thread(void *thpool);
static int wait_pool(m_thpool_t *pool, thpool_shutdown_t shutdown);
static inline bool has_space(m_thpool_t *pool);
static int add_threads(m_thpool_t *pool, int num);

static void *thpool_thread(void *thpool) {
    m_thpool_t *pool = (m_thpool_t *)thpool;
    
    while (true) {
        /* Lock must be taken to wait on conditional variable */
        pthread_mutex_lock(&(pool->lock));
        
        /* 
         * Wait on condition variable, check for spurious wakeups.
         * When returning from pthread_cond_wait(), we own the lock. 
         */
        while (m_queue_len(pool->tasks) == 0 && pool->shutdown == SHUTDOWN_NO) {
            pthread_cond_wait(&(pool->notify), &(pool->lock));
        }

        /*
         * User asked to quit either right now (SHUTDOWN_WAITCURR)
         * or after all jobs are processed (SHUTDOWN_WAITALL)
         */
        if (pool->shutdown && (pool->shutdown == SHUTDOWN_WAITCURR || m_queue_len(pool->tasks) == 0)) {
            break;
        }

        thpool_task_t *task = m_queue_dequeue(pool->tasks);

        /* Pool is no more used, unlock mutex */
        pthread_mutex_unlock(&(pool->lock));

        /* Actually call the task fn and then unref task */
        pool->running_tasks++;
        task->fn(task->arg);
        memhook._free(task);
        pool->running_tasks--;
    }
    
    pthread_mutex_unlock(&(pool->lock));
    return NULL;
}

static int wait_pool(m_thpool_t *pool, thpool_shutdown_t shutdown) {
    int ret = pthread_mutex_lock(&pool->lock);
    if (ret) {
        return ret;
    }

    pool->shutdown = shutdown;

    /* Wake up all worker threads and unlock mutex */
    ret = pthread_cond_broadcast(&pool->notify) + pthread_mutex_unlock(&pool->lock);
    if (ret == 0) {
        if (!(pool->flags & M_THPOOL_DETACHED)) {
            /* Join all worker threads */
            m_itr_foreach(pool->threads, {
                pthread_t *th = m_itr_get(itr);
                ret += pthread_join(*th, NULL);
            });
        }
        if (ret == 0) {
            /* There are no active threads anymore */
            pool->init_state &= ~INITED_STARTED;
        }
    }
    return ret;
}

static inline bool has_space(m_thpool_t *pool) {
    return pool->running_tasks < m_list_len(pool->threads);
}

static int add_threads(m_thpool_t *pool, int num) {
    /* Start worker threads */
    pthread_attr_t tattr;
    pthread_attr_init(&tattr);
    if (pool->flags & M_THPOOL_DETACHED) {
        pthread_attr_setdetachstate(&tattr, PTHREAD_CREATE_DETACHED);
    }
    int err = 0;
    for (int i = 0; i < num && err == 0; i++) {
        pthread_t *th = memhook._calloc(1, sizeof(pthread_t));
        err = pthread_create(th, &tattr, thpool_thread, (void *) pool);
        if (err == 0) {
            m_list_insert(pool->threads, th);
        } else {
            memhook._free(th);
        }
    }
    pthread_attr_destroy(&tattr);
    return err;
}

_public_ m_thpool_t *m_thpool_new(uint8_t thread_count, m_thpool_flags flags) {
    M_RET_ASSERT(thread_count > 0, NULL);
    
    m_thpool_t *pool = memhook._calloc(1, sizeof(m_thpool_t));
    M_RET_ASSERT(pool, NULL);

    int err = -ENOMEM;
    do {
        pool->threads = m_list_new(NULL, memhook._free);
        if (!pool->threads) {
            break;
        }
        pool->init_state |= INITED_THREADS;
        
        pool->tasks = m_queue_new(memhook._free);
        if (!pool->tasks) {
            break;
        }
        pool->init_state |= INITED_TASKS;
        
        /* Initialize mutex and conditional variable first */
        if (pthread_mutex_init(&(pool->lock), NULL) != 0) {
            break;
        }
        pool->init_state |= INITED_MUT;
        
        if (pthread_cond_init(&(pool->notify), NULL) != 0) {
            break;
        }
        pool->init_state |= INITED_COND;

        pool->max_threads = thread_count;
        pool->flags = flags;

        err = 0;
        if (!(flags & M_THPOOL_LAZY)) {
            /* Start worker threads */
            err = add_threads(pool, thread_count);
        }
    } while (false);
    
    /* Something went wrong; destroy */
    if (err != 0) {
        m_thpool_free(&pool, false);
    } else {
        pool->init_state |= INITED_STARTED;
    }
    return pool;
}

_public_ int m_thpool_add(m_thpool_t *pool, m_thpool_task task, void *arg) {
    M_PARAM_ASSERT(pool);
    M_PARAM_ASSERT(task);
    M_THREADS_ASSERT(pool, -EPERM);
    
    int ret = pthread_mutex_lock(&pool->lock);
    if (ret) {
        return ret;
    }

    /*
     * Lazy thread algorithm:
     * * IF number of currently running tasks is >= number of available threads,
     * * AND number of available threads is below max_threads,
     * THEN create a new thread were eventually new task will run.
     */
    if (pool->flags & M_THPOOL_LAZY) {
        if (!has_space(pool) && m_list_len(pool->threads) < pool->max_threads) {
            ret = add_threads(pool, 1);
            if (ret) {
                return ret;
            }
        }
    }

    /* Add task to queue */
    thpool_task_t *new_task = memhook._calloc(1, sizeof(thpool_task_t));
    new_task->fn = task;
    new_task->arg = arg;
    m_queue_enqueue(pool->tasks, new_task);
    ret = pthread_cond_signal(&pool->notify);

    const int unlock_ret = pthread_mutex_unlock(&pool->lock);
    if (unlock_ret == 0) {
        return ret;
    }
    return unlock_ret;
}

/* Returns number of enqueued tasks */
_public_ ssize_t m_thpool_length(m_thpool_t *pool) {
    M_PARAM_ASSERT(pool);
    M_THREADS_ASSERT(pool, -EPERM);
    
    int ret = pthread_mutex_lock(&pool->lock);
    if (ret) {
        return ret;
    }
    
    ssize_t len = m_queue_len(pool->tasks);
    
    const int unlock_ret = pthread_mutex_unlock(&pool->lock);
    if (unlock_ret == 0) {
        return len;
    }
    return unlock_ret;
}

/* Removes any non-running job from the queue */
_public_ ssize_t m_thpool_clear(m_thpool_t *pool) {
    M_PARAM_ASSERT(pool);
    M_THREADS_ASSERT(pool, -EPERM);
    
    int ret = pthread_mutex_lock(&pool->lock);
    if (ret) {
        return ret;
    }
    
    ret = m_queue_clear(pool->tasks);
    
    const int unlock_ret = pthread_mutex_unlock(&pool->lock);
    if (unlock_ret == 0) {
        return ret;
    }
    return unlock_ret;
}

/*
 * Calling m_thpool_free() without wait_all
 * means to just wait for currently executed jobs, then quit.
 * Else, means you desire to wait on any enqueued job
 * before destroying the thpool object.
 */
_public_ int m_thpool_free(m_thpool_t **pool, bool wait_all) {
    M_PARAM_ASSERT(pool && *pool);

    m_thpool_t *p = *pool;
    int ret = 0;

    for (int i = (p->init_state + 1) >> 1; i > 0 && ret == 0; i >>= 1) {
        switch (i) {
        case INITED_STARTED:
            ret = wait_pool(p, wait_all ? SHUTDOWN_WAITALL : SHUTDOWN_WAITCURR);
            break;
        case INITED_COND:
            ret = pthread_cond_destroy(&p->notify);
            break;
        case INITED_MUT:
            ret = pthread_mutex_destroy(&p->lock);
            break;
        case INITED_TASKS:
            ret = m_queue_free(&p->tasks);
            break;
        case INITED_THREADS:
            m_list_free(&p->threads);
            break;
        default:
            break;
        }
    }
    memhook._free(p);
    *pool = NULL;
    return 0;
}
