#include <stdlib.h>
#include <string.h>

#ifdef DUMMY_ALLOCATOR_LOG
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#define LOG(...) do { \
int len = snprintf(NULL, 0, __VA_ARGS__); \
char buf[len + 1]; \
sprintf(buf, __VA_ARGS__); \
syscall(SYS_write, 2, buf, len + 1); \
} while (0)
#else
#define LOG(...)
#endif

#define ARENA_SIZE (1 << 24)

typedef struct {
    size_t size;
    char data[];
} block_t;

static char arena[ARENA_SIZE];
static size_t used;

void *malloc(size_t size)
{
    LOG("malloc(%zd)\n", size);
    /* round up for alignment */
    size = (size + sizeof(size_t) - 1) / sizeof(size_t) * sizeof(size_t);

    size_t new_size = used + size + sizeof(block_t);
    if (new_size > ARENA_SIZE) {
        LOG("out of memory (%zd > %zd)\n", new_size, ARENA_SIZE);
        return NULL;
    }

    block_t *block = (block_t *)(arena + used);
    block->size = size;

    used = new_size;

    return block->data;
}

void free(void *ptr)
{
    LOG("free(%p)\n", ptr);
    /* no-op */
}

void *calloc(size_t nmemb, size_t size)
{
    LOG("calloc(%zd, %zd)\n", nmemb, size);
    return malloc(nmemb * size); /* arena is already zeroed */
}

void *realloc(void *ptr, size_t size)
{
    LOG("realloc(%p, %zd)\n", ptr, size);
    if (size == 0) {
        /* free is a no-op */
        return NULL;
    }

    if (!ptr) {
        return malloc(size);
    }

    block_t *block = (block_t*)ptr - 1;

    if (size > block->size) {
        ptr = malloc(size);
        if (!ptr) {
            return NULL;
        }

        memcpy(ptr, block->data, block->size);
    }

    return ptr;
}
