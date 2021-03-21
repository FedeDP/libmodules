#include "test_mem.h"
#include <module/mem.h>
#include <stddef.h>
#include <stdalign.h>
#include <stdlib.h>

void test_mem(void **state) {
    (void) state; /* unused */

    srand((unsigned int)time(0));
    /*
     * Check that allocating different block sizes
     * works fine without any alignment issue.
     */
    for (int i = 0; i < alignof(max_align_t); i++) {
        int mx = rand() % 20;
        void *data = m_mem_new(mx * i, NULL);
        assert_non_null(data);
        m_mem_unrefp(&data);
        assert_null(data);
    }
}