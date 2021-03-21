#include <module/thpool.h>
#include <string.h>
#include <stdlib.h>

#define NUM_THREADS     8
#define NUM_JOBS        64

static void *print(void *udata);

int main() {
    /* Create 8 threads, with unlimited number of jobs */
    m_thpool_t *pool = m_thpool_new(NUM_THREADS, 0);
    if (pool) {
        for (int i = 0; i < NUM_JOBS; i++) {
            char name[50] = {0};
            snprintf(name, sizeof(name) - 1, "Hello from job %d", i);
            m_thpool_add(pool, print, strdup(name));
        }
    }
    m_thpool_free(&pool, true);
    return 0;
}

static void *print(void *udata) {
    char *str = (char *)udata;
    printf("%s\n", str);
    free(udata);
    return NULL;
}
