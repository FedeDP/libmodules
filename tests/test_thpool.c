#include "test_thpool.h"
#include "module/thpool.h"
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

#define NUM_THREADS     8
#define NUM_JOBS        64

static void *inc(void *udata);
static void *inc_weird(void *udata);

static atomic_int ctr;

void test_thpool(void **state) {
    (void) state; /* unused */

    m_thpool_t *pool = m_thpool_new(NUM_THREADS, 0);
    assert_non_null(pool);

    for (int i = 0; i < NUM_JOBS; i++) {
        int ret = m_thpool_add(pool, NULL, NULL);
        assert_int_not_equal(ret, 0);

        ret = m_thpool_add(NULL, inc, NULL);
        assert_int_not_equal(ret, 0);

        ret = m_thpool_add(pool, inc, NULL);
        assert_int_equal(ret, 0);
    }

    /* wait any enqueued job */
    int ret = m_thpool_free(&pool, true);
    assert_int_equal(ret, 0);

    assert_int_equal(ctr, NUM_JOBS);
}

void test_thpool_lazy(void **state) {
    (void) state; /* unused */

    ctr = 0;

    m_thpool_t *pool = m_thpool_new(NUM_THREADS, M_THPOOL_LAZY);
    assert_non_null(pool);
    for (int i = 0; i < NUM_JOBS; i++) {
        int ret = m_thpool_add(pool, inc, NULL);
        assert_int_equal(ret, 0);
    }

    int ret = m_thpool_free(&pool, true);
    assert_int_equal(ret, 0);

    assert_int_equal(ctr, NUM_JOBS);
}

void test_thpool_weird_conditions(void **state) {
    (void) state; /* unused */

    ctr = 0;

    m_thpool_t *pool = m_thpool_new(0, 0);
    assert_null(pool);

    /** First Test:
     *  create a thpool and free it immediately, without adding any job.
     */

    pool = m_thpool_new(NUM_THREADS, 0);
    assert_non_null(pool);

    /* No jobs... */
    int ret = m_thpool_free(&pool, false);
    assert_int_equal(ret, 0);

    /** Second Test:
     *  create a thpool then add NUM_JOBS tasks that each sleep 1s;
     *  finally, free the thpool without awaiting all jobs (just the running ones)
     *  Test that only NUM_THREADS jobs were actually executed (ie: 1 for each thread)
     */

    pool = m_thpool_new(NUM_THREADS, 0);
    assert_non_null(pool);

    for (int i = 0; i < NUM_JOBS; i++) {
        ret = m_thpool_add(pool, inc_weird, NULL);
        assert_int_equal(ret, 0);
    }

    /*
     * Give some time to actually allow threads
     * to receive their tasks before the thpool is freed
     */
    usleep(250000);

    /*
     * We won't wait all jobs, just first NUM_THREADS (1 for each thread)
     */
    ret = m_thpool_free(&pool, false);
    assert_int_equal(ret, 0);

    assert_int_equal(ctr, NUM_THREADS);
}

static void *inc(void *udata) {
    ctr++;
    return NULL;
}

static void *inc_weird(void *udata) {
    sleep(1);
    ctr++;
    return NULL;
}