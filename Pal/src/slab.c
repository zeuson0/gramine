/* SPDX-License-Identifier: LGPL-3.0-or-later */
/* Copyright (C) 2014 Stony Brook University */

/*
 * This file contains implementation of PAL's internal memory allocator.
 */

#include "api.h"
#include "asan.h"
#include "pal.h"
#include "pal_error.h"
#include "pal_internal.h"
#include "spinlock.h"

static spinlock_t g_slab_mgr_lock = INIT_SPINLOCK_UNLOCKED;

#define SYSTEM_LOCK()   spinlock_lock(&g_slab_mgr_lock)
#define SYSTEM_UNLOCK() spinlock_unlock(&g_slab_mgr_lock)
#define SYSTEM_LOCKED() spinlock_is_locked(&g_slab_mgr_lock)

static inline void* __malloc(size_t size);
static inline void __free(void* addr, size_t size);
#define system_malloc(size) __malloc(size)
#define system_free(addr, size) __free(addr, size)

#include "slabmgr.h"

static inline void* __malloc(size_t size) {
    void* addr = NULL;

    size = ALIGN_UP(size, MIN_MALLOC_ALIGNMENT);

    int ret = _DkVirtualMemoryAlloc(&addr, ALLOC_ALIGN_UP(size), PAL_ALLOC_INTERNAL,
              PAL_PROT_READ | PAL_PROT_WRITE);
    if (ret < 0) {
        log_error("*** Out-of-memory in PAL (try increasing `loader.pal_internal_mem_size`) ***");
        _DkProcessExit(ENOMEM);
    }
#ifdef ASAN
    asan_poison_region((uintptr_t)addr, ALLOC_ALIGN_UP(size), ASAN_POISON_HEAP_LEFT_REDZONE);
#endif

    return addr;
}

static inline void __free(void* addr, size_t size) {
    if (!addr)
        return;

    size = ALIGN_UP(size, MIN_MALLOC_ALIGNMENT);

#ifdef ASAN
    /* Unpoison the memory before unmapping it */
    asan_unpoison_region((uintptr_t)addr, ALLOC_ALIGN_UP(size));
#endif

    _DkVirtualMemoryFree(addr, ALLOC_ALIGN_UP(size));
}

static SLAB_MGR g_slab_mgr = NULL;

void init_slab_mgr(void) {
    assert(!g_slab_mgr);

    g_slab_mgr = create_slab_mgr();
    if (!g_slab_mgr)
        INIT_FAIL(PAL_ERROR_NOMEM, "cannot initialize slab manager");
}

void* malloc(size_t size) {
    void* ptr = slab_alloc(g_slab_mgr, size);

#ifdef DEBUG
    /* In debug builds, try to break code that uses uninitialized heap
     * memory by explicitly initializing to a non-zero value. */
    if (ptr)
        memset(ptr, 0xa5, size);
#endif

    if (!ptr) {
        /*
         * Normally, the PAL should not run out of memory.
         * If malloc() failed internally, we cannot handle the
         * condition and must terminate the current process.
         */
        log_error("******** Out-of-memory in PAL ********");
        _DkProcessExit(ENOMEM);
    }
    return ptr;
}

// Copies data from `mem` to a newly allocated buffer of a specified size.
void* malloc_copy(const void* mem, size_t size) {
    void* nmem = malloc(size);

    if (nmem)
        memcpy(nmem, mem, size);

    return nmem;
}

void* calloc(size_t nmem, size_t size) {
    void* ptr = malloc(nmem * size);

    if (ptr)
        memset(ptr, 0, nmem * size);

    return ptr;
}

void free(void* ptr) {
    if (!ptr)
        return;
    slab_free(g_slab_mgr, ptr);
}
