#include "test_commons.h"

void test_ctx_register_NULL_name(void **state);
void test_ctx_register(void **state);
void test_ctx_deregister_NULL_name(void **state);
void test_ctx_deregister(void **state);
void test_ctx_set_logger_NULL_logger(void **state);
void test_ctx_set_logger(void **state);
void test_ctx_dump(void **state);
void test_ctx_quit_no_loop(void **state);
void test_ctx_loop(void **state);
void test_ctx_dispatch(void **state);
void test_ctx_mod_deregister_during_loop(void **state);
