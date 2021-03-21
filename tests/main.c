#include "test_mod.h"
#include "test_ctx.h"
#include "test_map.h"
#include "test_stack.h"
#include "test_queue.h"
#include "test_list.h"
#include "test_bst.h"
#include "test_evt_ref.h"
#include "test_perf.h"
#include "test_mem.h"
#include "test_thpool.h"

int main(void) {
    const struct CMUnitTest tests[] = {
        /* Test module_register failures */
        cmocka_unit_test(test_mod_register_NULL_name),
        cmocka_unit_test(test_mod_register_NULL_self),
        cmocka_unit_test(test_mod_register_NULL_hook),

        /* Test ctx register */
        cmocka_unit_test(test_ctx_register_NULL_name),
        cmocka_unit_test(test_ctx_register),
        
        /* Finally regiter module */
        cmocka_unit_test(test_mod_register),
        
        /* We have a module and its ctx now! */
        
        /*
         * Check that a module cannot be registered more that one time 
         * if module_deregister does not get called before
         */
        cmocka_unit_test(test_mod_register_already_registered),
        /* 
         * Check that another module register fails 
         * if module has same name and same context 
         */
        cmocka_unit_test(test_mod_register_same_name),
        
        /* Test modules_ API */
        cmocka_unit_test(test_ctx_set_logger_NULL_logger),
        cmocka_unit_test(test_ctx_set_logger),
        cmocka_unit_test(test_ctx_quit_no_loop),
        cmocka_unit_test(test_ctx_dump),
        
        /* Test module state setters */
        cmocka_unit_test(test_mod_start_NULL_self),
        cmocka_unit_test(test_mod_start),
        cmocka_unit_test(test_mod_pause_NULL_self),
        cmocka_unit_test(test_mod_pause),
        cmocka_unit_test(test_mod_resume_NULL_self),
        cmocka_unit_test(test_mod_resume),
        cmocka_unit_test(test_mod_stop_NULL_self),
        cmocka_unit_test(test_mod_stop),
        cmocka_unit_test(test_mod_start_NULL_self),
        cmocka_unit_test(test_mod_start),
        
        /* Test module logger */
        cmocka_unit_test(test_mod_log_NULL_self),
        cmocka_unit_test(test_mod_log),
        
        /* Test module dumper */
        cmocka_unit_test(test_mod_dump_NULL_self),
        cmocka_unit_test(test_mod_dump),
        
        /* Test module become */
        cmocka_unit_test(test_mod_become_NULL_self),
        cmocka_unit_test(test_mod_become_NULL_func),
        cmocka_unit_test(test_mod_become),
        
        /* Test module unbecome */
        cmocka_unit_test(test_mod_unbecome_NULL_self),
        cmocka_unit_test(test_mod_unbecome),
        
        /* Test fd add/rm */
        cmocka_unit_test(test_mod_add_wrong_fd),
        cmocka_unit_test(test_mod_add_fd_NULL_self),
        cmocka_unit_test(test_mod_add_fd),
        cmocka_unit_test(test_mod_rm_wrong_fd),
        cmocka_unit_test(test_mod_rm_wrong_fd_2),
        cmocka_unit_test(test_mod_rm_fd_NULL_self),
        cmocka_unit_test(test_mod_rm_fd),
        
        /* Test module subscribe */
        cmocka_unit_test(test_mod_subscribe_NULL_topic),
        cmocka_unit_test(test_mod_subscribe_NULL_self),
        
        /* Now topic has been registered, subscribe should work */
        cmocka_unit_test(test_mod_subscribe),
        
        /* Test module ref */
        cmocka_unit_test(test_mod_ref_NULL_name),
        cmocka_unit_test(test_mod_ref_unexhistent_name),
        cmocka_unit_test(test_mod_ref),
        
        /* Test module tell */
        cmocka_unit_test(test_mod_tell_NULL_recipient),
        cmocka_unit_test(test_mod_tell_NULL_self),
        cmocka_unit_test(test_mod_tell_NULL_msg),
        cmocka_unit_test(test_mod_tell),
        
        /* Test module publish */
        cmocka_unit_test(test_mod_publish_NULL_self),
        cmocka_unit_test(test_mod_publish_NULL_msg),
        cmocka_unit_test(test_mod_publish_NULL_topic),
        cmocka_unit_test(test_mod_publish),
        
        /* Test module broadcast */
        cmocka_unit_test(test_mod_broadcast_NULL_self),
        cmocka_unit_test(test_mod_broadcast_NULL_msg),
        cmocka_unit_test(test_mod_broadcast),
        
        /* We have now 3 messages waiting for us (tell, publish, broadcast). Check. */
        cmocka_unit_test(test_ctx_loop),
        
        /* We have 0 messages now */
        cmocka_unit_test(test_ctx_dispatch),
        
        /* Test module unref */
        cmocka_unit_test(test_mod_unref_NULL_ref),
        cmocka_unit_test(test_mod_unref),
        
        /* Test module_deregister failures */
        cmocka_unit_test(test_mod_deregister_NULL_self),
        
        /* Actually deregister our module */
        cmocka_unit_test(test_mod_deregister),
        
        /* Test that if init returns false, module is stopped */
        cmocka_unit_test(test_mod_false_init),

        /* Test ctx deregister */
        cmocka_unit_test(test_ctx_deregister_NULL_name),
        cmocka_unit_test(test_ctx_deregister),
        
        /* Test modules_ API: it should fail now */
        cmocka_unit_test(test_ctx_quit_no_loop), // here context is already destroyed

        /*
         * Test that calling m_mod_deregister() inside on_evt() user callback
         * on current module won't cause any problem.
         * Moreover, test that if all modules in a context are deregistered,
         * the context gets automatically deregistered too, even if it was looping.
         */
        cmocka_unit_test(test_ctx_mod_deregister_during_loop),

        /* Test Map API */
        cmocka_unit_test(test_map_put),
        cmocka_unit_test(test_map_get),
        cmocka_unit_test(test_map_length),
        cmocka_unit_test(test_map_iterator),
        cmocka_unit_test(test_map_iterate),
        cmocka_unit_test(test_map_remove),
        cmocka_unit_test(test_map_clear),
        cmocka_unit_test(test_map_free),
        cmocka_unit_test(test_map_stress),

        /* Test Stack API */
        cmocka_unit_test(test_stack_push),
        cmocka_unit_test(test_stack_peek),
        cmocka_unit_test(test_stack_length),
        cmocka_unit_test(test_stack_iterator),
        cmocka_unit_test(test_stack_pop),
        cmocka_unit_test(test_stack_free),

        /* Test Queue API */
        cmocka_unit_test(test_queue_enqueue),
        cmocka_unit_test(test_queue_peek),
        cmocka_unit_test(test_queue_length),
        cmocka_unit_test(test_queue_iterator),
        cmocka_unit_test(test_queue_dequeue),
        cmocka_unit_test(test_queue_free),
        
        /* Test List API */
        cmocka_unit_test(test_list_insert),
        cmocka_unit_test(test_list_length),
        cmocka_unit_test(test_list_iterator),
        cmocka_unit_test(test_list_find),
        cmocka_unit_test(test_list_remove),
        cmocka_unit_test(test_list_clear),
        cmocka_unit_test(test_list_free),
        cmocka_unit_test(test_list_int),
        
        /* Test BST API */
        cmocka_unit_test(test_bst_insert),
        cmocka_unit_test(test_bst_length),
        cmocka_unit_test(test_bst_find),
        cmocka_unit_test(test_bst_remove),
        cmocka_unit_test(test_bst_iterator),
        cmocka_unit_test(test_bst_traverse),
        cmocka_unit_test(test_bst_clear),
        cmocka_unit_test(test_bst_free),
        
        /* Test m_evt_t reference */
        cmocka_unit_test(test_evt_ref),
        
        /* Test poll plugin performance */
        cmocka_unit_test(test_poll_perf),

        cmocka_unit_test(test_mem),

        /* Test thpool API */
        cmocka_unit_test(test_thpool),
        cmocka_unit_test(test_thpool_lazy),
        cmocka_unit_test(test_thpool_weird_conditions),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
