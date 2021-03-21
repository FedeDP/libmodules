#include "test_bst.h"
#include <module/itr.h>

static m_bst_t *my_t;
static int arr[100];

static int int_cmp(void *userdata, void *node_data) {
    int a = *((int *)userdata);
    int b = *((int *)node_data);
    return (a - b);
}

static int traverse_pre_cb(void *userptr, void *node_data) {
    static int counter = 0;
    int val = *((int *) node_data);
    printf("%d\n", val);
    if (++counter == 5) {
        return 5; // check that 5 is not forwarded but traverse is stopped
    }
    return 0;
}

static int traverse_post_cb(void *userptr, void *node_data) {
    int val = *((int *) node_data);
    printf("%d\n", val);
    return -1; // check that -1 is forwarded
}

static int traverse_in_cb(void *userptr, void *node_data) {
    int val = *((int *) node_data);
    printf("%d\n", val);
    return 0;
}

void test_bst_insert(void **state) {
    (void) state; /* unused */
    
    int ret = m_bst_insert(my_t, &arr[0]);
    assert_false(ret == 0);
    
    my_t = m_bst_new(int_cmp, NULL);
    
    /* NULL value */
    ret = m_bst_insert(my_t, NULL);
    assert_false(ret == 0);
    
    srand(time(NULL));
    for (int i = 0; i < 100; i++) {
        do {
            arr[i] = rand() % 10000;
        }
        while (m_bst_insert(my_t, &arr[i]) != 0);
    }
}

void test_bst_length(void **state) {
    (void) state; /* unused */
    
    int len = m_bst_len(NULL);
    assert_false(len > 0);
    
    len = m_bst_len(my_t);
    assert_int_equal(len, 100);
}

void test_bst_find(void **state) {
    (void) state; /* unused */
    
    void *data = m_bst_find(NULL, NULL);
    assert_null(data);
    
    data = m_bst_find(my_t, NULL);
    assert_null(data);
    
    int c = -1;
    data = m_bst_find(my_t, &c);
    assert_null(data);
    
    data = m_bst_find(my_t, &arr[2]);
    assert_non_null(data);
    assert_ptr_equal(data, &arr[2]);
    
    c = arr[1];
    data = m_bst_find(my_t, &c);
    assert_non_null(data);
    assert_ptr_equal(data, &arr[1]);
}

void test_bst_remove(void **state) {
    (void) state; /* unused */
    
    int ret = m_bst_remove(NULL, NULL);
    assert_false(ret == 0);
    
    ret = m_bst_remove(my_t, NULL);
    assert_false(ret == 0);
    
    ret = m_bst_remove(my_t, &arr[5]);
    assert_int_equal(ret, 0);
    
    ret = m_bst_remove(my_t, &arr[25]);
    assert_int_equal(ret, 0);
    
    ret = m_bst_remove(my_t, &arr[45]);
    assert_int_equal(ret, 0);
    
    ret = m_bst_remove(my_t, &arr[65]);
    assert_int_equal(ret, 0);
    
    ret = m_bst_remove(my_t, &arr[85]);
    assert_int_equal(ret, 0);
    
    size_t len = m_bst_len(my_t);
    assert_int_equal(len, 95);
}

void test_bst_iterator(void **state) {
    (void) state; /* unused */
    
    /* NULL list */
    m_bst_itr_t *it = m_bst_itr_new(NULL);
    assert_null(it);
    
    int count = m_bst_len(my_t);
    m_itr_foreach(my_t, {
        int *val = m_itr_get(itr);
        printf("%d\n", *val);
        if (rand() % 2 == 1) {
            m_itr_rm(itr);
        }
        count--;
    });
    assert_int_equal(count, 0);
}

void test_bst_traverse(void **state) {
    (void) state; /* unused */
    
    int ret = m_bst_traverse(NULL, M_BST_PRE, traverse_pre_cb, NULL);
    assert_false(ret == 0);
    
    ret = m_bst_traverse(my_t, -1, traverse_pre_cb, NULL);
    assert_false(ret == 0);
    
    ret = m_bst_traverse(my_t, -1, NULL, NULL);
    assert_false(ret == 0);
    
    printf("PREORDER (only first 5):\n");
    ret = m_bst_traverse(my_t, M_BST_PRE, traverse_pre_cb, NULL);
    assert_int_equal(ret, 0);
    
    printf("POSTORDER (only first):\n");
    ret = m_bst_traverse(my_t, M_BST_POST, traverse_post_cb, NULL);
    assert_int_equal(ret, -1);
    
    printf("INORDER:\n");
    ret = m_bst_traverse(my_t, M_BST_IN, traverse_in_cb, NULL);
    assert_int_equal(ret, 0);
}

void test_bst_clear(void **state) {
    (void) state; /* unused */
    
    int ret = m_bst_clear(NULL);
    assert_false(ret == 0);
    
    ret = m_bst_clear(my_t);
    assert_true(ret == 0);
    
    int len = m_bst_len(my_t);
    assert_int_equal(len, 0);
}

void test_bst_free(void **state) {
    (void) state; /* unused */
    
    int ret = m_bst_free(NULL);
    assert_false(ret == 0);
    
    ret = m_bst_free(&my_t);
    assert_true(ret == 0);
    assert_null(my_t);
}
