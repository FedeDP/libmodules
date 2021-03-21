#include "test_stack.h"
#include <module/itr.h>

static m_stack_t *my_st;
static int val1 = 1;
static int val2 = 2;
static char val3[] = "Hello World";

void test_stack_push(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int ret = m_stack_push(my_st, &val1);
    assert_false(ret == 0);
    
    my_st = m_stack_new(NULL);
    
    /* NULL value */
    ret = m_stack_push(my_st, NULL);
    assert_false(ret == 0);
    
    ret = m_stack_push(my_st, &val1);
    assert_true(ret == 0);
    
    ret = m_stack_push(my_st, &val2);
    assert_true(ret == 0);
    
    ret = m_stack_push(my_st, val3);
    assert_true(ret == 0);
}

void test_stack_peek(void **state) {
    (void) state; /* unused */
    
    char *v = m_stack_peek(NULL);
    assert_null(v);
    
    v = m_stack_peek(my_st);
    assert_non_null(v);
    assert_string_equal(v, "Hello World");
}

void test_stack_length(void **state) {
    (void) state; /* unused */
    
    int len = m_stack_len(NULL);
    assert_false(len > 0);
    
    len = m_stack_len(my_st);
    assert_int_equal(len, 3);
}

void test_stack_iterator(void **state) {
    (void) state; /* unused */
    
    /* NULL m_stack */
    m_stack_itr_t *itr = m_stack_itr_new(NULL);
    assert_null(itr);
    
    itr = m_itr_new(my_st);
    assert_non_null(itr);
    
    int count = m_stack_len(my_st);
    while (itr) {
        printf("%p\n", m_itr_get(itr));
        if (count % 2 == 0) {
            m_itr_rm(itr);
        }
        m_itr_next(&itr);
        count--;
        
    }
    
    assert_int_equal(count, 0);
    assert_null(itr);
}

void test_stack_pop(void **state) {
    (void) state; /* unused */
    
    void *ptr = m_stack_pop(NULL);
    assert_null(ptr);
    
    ptr = m_stack_pop(my_st);
    assert_non_null(ptr);
    assert_string_equal(ptr, "Hello World");
    
    int len = m_stack_len(my_st);
    assert_int_equal(len, 1); // one element left
}

void test_stack_clear(void **state) {
    (void) state; /* unused */
    
    int ret = m_stack_clear(NULL);
    assert_false(ret == 0);
    
    ret = m_stack_clear(my_st);
    assert_true(ret == 0);
    
    int len = m_stack_len(my_st);
    assert_int_equal(len, 0);
}

void test_stack_free(void **state) {
    (void) state; /* unused */
    
    int ret = m_stack_free(NULL);
    assert_false(ret == 0);
    
    ret = m_stack_free(&my_st);
    assert_true(ret == 0);
    assert_null(my_st);
}
