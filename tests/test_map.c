#include "test_map.h"
#include <module/itr.h>

static m_map_t *my_map;
static int val = 5;
static int updated_val = 45;
static int count;

void test_map_put(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int ret = m_map_put(my_map, "key", &val);
    assert_false(ret == 0);
    
    my_map = m_map_new(M_MAP_VAL_ALLOW_UPDATE, NULL);
    
    /* NULL value */
    ret = m_map_put(my_map, "key", NULL);
    assert_false(ret == 0);
    
    /* NULL key */
    ret = m_map_put(my_map, NULL, &val);
    assert_false(ret == 0);
    
    ret = m_map_put(my_map, "key", &val);
    assert_true(ret == 0);
    assert_true(m_map_contains(my_map, "key"));
    assert_int_equal(m_map_len(my_map), 1);

    /* Update val; check that map size was not increased */
    ret = m_map_put(my_map, "key", &updated_val);
    assert_true(ret == 0);
    assert_int_equal(m_map_len(my_map), 1);

    /* Check that value was updated - M_MAP_VAL_ALLOW_UPDATE flag was passed */
    int *v = m_map_get(my_map, "key");
    assert_non_null(v);
    assert_int_equal(*v, updated_val);

    ret = m_map_put(my_map, "key2", &val);
    assert_true(ret == 0);
    assert_true(m_map_contains(my_map, "key2"));
}

void test_map_get(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int *value = m_map_get(NULL, "key");
    assert_null(value);
    
    /* NULL key */
    value = m_map_get(my_map, NULL);
    assert_null(value);
    
    /* Unhexistent key */
    value = m_map_get(my_map, "keykey");
    assert_null(value);
    
    value = m_map_get(my_map, "key");
    assert_non_null(value);
    assert_int_equal(*value, updated_val);
}

void test_map_length(void **state) {
    (void) state; /* unused */
    
    int len = m_map_len(NULL);
    assert_false(len >= 0);
    
    len = m_map_len(my_map);
    assert_int_equal(len, 2);
}

void test_map_iterator(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    m_map_itr_t *itr = m_map_itr_new(NULL);
    assert_null(itr);
    
    itr = m_itr_new(my_map);
    assert_non_null(itr);
    
    count = m_map_len(my_map);
    while (itr) {
        count--;
        printf("%s -> %p\n", m_map_itr_get_key(itr), m_map_itr_get_data(itr));
        m_itr_next(&itr);
    }
    assert_int_equal(count, 0);
    assert_null(itr);
}

int iterate_cb(void *userptr, const char *key, void *data) {
    count++;
    return 0;
}

void test_map_iterate(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int ret = m_map_iterate(NULL, iterate_cb, NULL);
    assert_false(ret == 0);
    
    /* NULL cb */
    ret = m_map_iterate(my_map, NULL, NULL);
    assert_false(ret == 0);
    
    ret = m_map_iterate(my_map, iterate_cb, NULL);
    assert_true(ret == 0);
    assert_int_equal(count, m_map_len(my_map));
}

void test_map_remove(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int ret = m_map_remove(NULL, "key");
    assert_false(ret == 0);
    
    /* NULL key */
    ret = m_map_remove(my_map, NULL);
    assert_false(ret == 0);
    
    /* NULL key */
    ret = m_map_remove(my_map, "key");
    assert_true(ret == 0);
    
    int len = m_map_len(my_map);
    assert_int_equal(len, 1); // "key2" still inside
}

void test_map_clear(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int ret = m_map_clear(NULL);
    assert_false(ret == 0);
    
    ret = m_map_clear(my_map);
    assert_true(ret == 0);
    
    int len = m_map_len(my_map);
    assert_int_equal(len, 0);
}

void test_map_free(void **state) {
    (void) state; /* unused */
    
    /* NULL map */
    int ret = m_map_free(NULL);
    assert_false(ret == 0);
    
    ret = m_map_free(&my_map);
    assert_true(ret == 0);
    assert_null(my_map);
}

void test_map_stress(void **state) {
    (void) state; /* unused */

    clock_t begin_tell = clock();

    int ret;
    my_map = m_map_new(M_MAP_KEY_DUP, NULL);
    const int size = 1000000;
    for (int i = 0; i < size; i++) {
        char key[20];
        snprintf(key, sizeof(key), "key%d", i);
        ret = m_map_put(my_map, key, "top");
        assert_true(ret == 0);
        assert_true(m_map_contains(my_map, key));
    }
    assert_int_equal(m_map_len(my_map), size);
    
    count = 0;
    ret = m_map_iterate(my_map, iterate_cb, NULL);
    assert_true(ret == 0);
    assert_int_equal(count, m_map_len(my_map));
    
    ret = m_map_free(&my_map);
    assert_true(ret == 0);
    assert_null(my_map);

    clock_t end_tell = clock();
    double time_spent = (double)(end_tell - begin_tell);
    printf("Map stress test took %.2lf us\n", time_spent);
}
