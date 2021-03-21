#include "priv.h"
#include <stddef.h>
#include <stdalign.h>

#define ALIGN_UP(x)             __ALIGN_MASK(x, (__typeof__(x))(alignof(max_align_t)) - 1)
#define __ALIGN_MASK(x, mask)    (((x) + (mask)) &~ (mask))

typedef struct {
    size_t refs;        // Number of reference for this memory object
    size_t size;        // size of user data, returned by m_mem_size()
    m_ref_dtor dtor;    // Dtor for the memory object
    uint8_t data[];     // Flexible array member for user data
} mem_header_t;

static inline mem_header_t *get_header(uint8_t *src) {
    const uint8_t align_shift = src[-1];
    return (mem_header_t *)(src - sizeof(mem_header_t) - align_shift);
}

/** Private API **/

void mem_dtor(void *src) {
    m_mem_unref(src);
}

/** Public API **/

/* Create new ref counted memory area */
_public_ void *m_mem_new(size_t size, m_ref_dtor dtor) {
    /* Always use maximum alignment for the platform */
    const size_t total_size = sizeof(mem_header_t) + size;
    size_t total_size_aligned = ALIGN_UP(total_size);
    uint8_t align_shift = total_size_aligned - total_size;
    if (align_shift == 0) {
        /* Add a new aligned block; it is needed to later store alignment information */
        align_shift = alignof(max_align_t);
    }
    mem_header_t *header = memhook._calloc(1, total_size + align_shift);
    if (header) {
        header->refs = 1;
        header->dtor = dtor;
        header->size = size;
        uint8_t *data = header->data + align_shift;
        /* Store alignment shift */
        data[-1] = align_shift;
        return data;
    }
    return NULL;
}

/* Gain a new ref on a memory area */
_public_ void *m_mem_ref(void *src) {
    if (src) {
        mem_header_t *header = get_header(src);
        header->refs++;
    }
    return src;
}

/* Remove a ref from a memory area */
_public_ void *m_mem_unref(void *src) {
    if (src) {
        mem_header_t *header = get_header(src);
        if (--header->refs == 0) {
            if (header->dtor) {
                header->dtor(src); // destroy private data
            }
            memhook._free(header);
        }
    }
    return NULL;
}

_public_ void m_mem_unrefp(void **src) {
    if (src) {
        *src = m_mem_unref(*src);
    }
}

_public_ size_t m_mem_size(void *src) {
    if (src) {
        mem_header_t *header = get_header(src);
        return header->size;
    }
    return 0;
}
