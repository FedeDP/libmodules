#include "test_list.h"
#include <module/itr.h>
#include <stdlib.h>

static m_list_t *my_l;
static int val1 = 1;
static int val2 = 2;
static int val3 = 3;

void test_list_insert(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int ret = m_list_insert(my_l, &val1);
    assert_false(ret == 0);
    
    my_l = m_list_new(NULL, NULL);
    
    /* NULL value */
    ret = m_list_insert(my_l, NULL);
    assert_false(ret == 0);
    
    ret = m_list_insert(my_l, &val1);
    assert_true(ret == 0);
    
    ret = m_list_insert(my_l, &val2);
    assert_true(ret == 0);
    
    ret = m_list_insert(my_l, &val3);
    assert_true(ret == 0);
}

void test_list_length(void **state) {
    (void) state; /* unused */
    
    int len = m_list_len(NULL);
    assert_false(len > 0);
    
    len = m_list_len(my_l);
    assert_int_equal(len, 3);
}

void test_list_iterator(void **state) {
    (void) state; /* unused */
    
    /* NULL list */
    m_list_itr_t *itr = m_list_itr_new(NULL);
    assert_null(itr);
    
    itr = m_list_itr_new(my_l);
    assert_non_null(itr);
    
    int count = m_list_len(my_l);

    while (count) {
        count--;
        printf("%p\n", m_itr_get(itr));
        
        /* Insert a node */
        int ret = m_list_itr_insert(itr, &val1);
        assert_true(ret == 0);
        
        /* Remove previously inserted node */
        ret = m_itr_rm(itr);
        assert_true(ret == 0);
        
        m_itr_next(&itr);
    }
    
    assert_int_equal(count, 0);
    assert_null(itr);
}

void test_list_find(void **state) {
    void *data = m_list_find(NULL, NULL);
    assert_null(data);
    
    data = m_list_find(my_l, NULL);
    assert_null(data);
    
    int c = 0;
    data = m_list_find(my_l, &c);
    assert_null(data);
    
    data = m_list_find(my_l, &val2);
    assert_non_null(data);
    assert_ptr_equal(data, &val2);
}

void test_list_remove(void **state) {
    (void) state; /* unused */
    
    int ret = m_list_remove(NULL, NULL);
    assert_false(ret == 0);
    
    ret = m_list_remove(my_l, NULL);
    assert_false(ret == 0);
    
    ret = m_list_remove(my_l, &val1);
    assert_true(ret == 0);
    
    ret = m_list_remove(my_l, &val2);
    assert_true(ret == 0);
    
    int len = m_list_len(my_l);
    assert_int_equal(len, 1); // one element left, ie: val3
}

void test_list_clear(void **state) {
    (void) state; /* unused */
    
    int ret = m_list_clear(NULL);
    assert_false(ret == 0);
    
    ret = m_list_clear(my_l);
    assert_true(ret == 0);
    
    int len = m_list_len(my_l);
    assert_int_equal(len, 0);
}

void test_list_free(void **state) {
    (void) state; /* unused */
    
    int ret = m_list_free(NULL);
    assert_false(ret == 0);
    
    ret = m_list_free(&my_l);
    assert_true(ret == 0);
    assert_null(my_l);
}

static int int_match(void *my_data, void *list_data) {
    int a = *((int *)my_data);
    int b = *((int *)list_data);
    return !(a == b);
}

void test_list_int(void **state) {
    int ret;
    my_l = m_list_new(int_match, free);
    for (int i = 0; i < 10; i++) {
        int *p = malloc(sizeof(int));
        *p = i;
        ret = m_list_insert(my_l, p);
        assert_true(ret == 0);
    }
    int len = m_list_len(my_l);
    assert_int_equal(len, 10);

    int val = 5;
    int *data = m_list_find(my_l, &val);
    assert_non_null(data);
    assert_int_equal(*data, 5);

    val = 7;
    ret = m_list_remove(my_l, &val);
    assert_int_equal(ret, 0);
    len = m_list_len(my_l);
    assert_int_equal(len, 9);

    val = 9;
    ret = m_list_remove(my_l, &val);
    assert_int_equal(ret, 0);
    len = m_list_len(my_l);
    assert_int_equal(len, 8);

    val = 10;
    ret = m_list_remove(my_l, &val);
    assert_false(ret == 0); // nonexistent!
    len = m_list_len(my_l);
    assert_int_equal(len, 8);

    ret = m_list_free(&my_l);
    assert_true(ret == 0);
    assert_null(my_l);
}
