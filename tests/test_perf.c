#include "test_perf.h"
#include <module/mod.h>
#include <module/ctx.h>
#include <string.h>
#include <time.h>

#define MAX_LEN 5000

static void my_recv(m_mod_t *mod, const m_evt_t *msg);

static m_mod_t *mod;
static int ctr;

void test_poll_perf(void **state) {
    (void) state; /* unused */

    test_ctx = NULL;

    int ret = m_ctx_register("perf", &test_ctx, 0, NULL);
    assert_true(ret == 0);
    assert_non_null(test_ctx);

    m_src_thresh_t thresh = { .activity_freq = 10.0 };
    m_src_thresh_t alarm = {0};
        
    m_mod_hook_t hook = { .on_evt = my_recv };
    ret = m_mod_register("testName", test_ctx, &mod, &hook, 0, NULL);
    assert_true(ret == 0);
    assert_non_null(mod);
    assert_true(m_mod_is(mod, M_MOD_IDLE));

    ret = m_mod_src_register(mod, &thresh, 0, &alarm);
    assert_int_equal(ret, 0);

    // test that it can be deregistered
    ret = m_mod_src_deregister(mod, &thresh);
    assert_int_equal(ret, 0);

    ret = m_mod_src_register(mod, &thresh, 0, &alarm);
    assert_int_equal(ret, 0);

    m_mod_start(mod);

    clock_t begin_tell = clock();
    for (int i = 0; i < MAX_LEN; i++) {
        m_mod_ps_tell(mod, mod, "Hello World", 0);
    }
    clock_t end_tell = clock();
    double time_spent = (double)(end_tell - begin_tell);
    printf("Messages feeding took %.2lf us\n", time_spent);
    
    m_ctx_loop(test_ctx);
    
    clock_t end_recv = clock();
    time_spent = (double)(end_recv - end_tell);
    printf("Messages fetching took %.2lf us\n", time_spent);

    assert_true(alarm.activity_freq > 0);
    printf("Was warned as activity was beyond thresh: %.2lf > %.2lf.\n", alarm.activity_freq, thresh.activity_freq);

    m_mod_deregister(&mod);
}

static void my_recv(m_mod_t *mod, const m_evt_t *msg) {
    if (msg->type == M_SRC_TYPE_PS && msg->ps_evt->type == M_PS_USER && ++ctr == MAX_LEN) {
        m_ctx_quit(test_ctx, 0);
    } else if (msg->type == M_SRC_TYPE_THRESH) {
        m_src_thresh_t *alarm = (m_src_thresh_t *)msg->userdata;
        alarm->activity_freq = msg->thresh_evt->activity_freq;
        alarm->inactive_ms = msg->thresh_evt->inactive_ms;
    }
}
