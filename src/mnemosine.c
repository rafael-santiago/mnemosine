/*
 *                          Copyright (C) 2019 by Rafael Santiago
 *
 * Use of this source code is governed by GPL-v2 license that can
 * be found in the COPYING file.
 *
 */
#include <mnemosine.h>
#include <string.h>
#ifndef _WIN32
# if !defined(MNEMOSINE_NO_PTHREAD)
#  include <pthread.h>
# endif
#else
# include <windows.h>
#endif

#if !defined(MNEMOSINE_NO_STATIC_HEAP)
# if !defined(MNEMOSINE_STATIC_HEAP_SIZE)
#  define MNEMOSINE_STATIC_HEAP_SIZE     mnemosine_size_mb(5) // INFO(Rafael): 5MB.
# endif
#endif

#define mnemosine_hash(mn, a) ( (unsigned char *)(a) - (mn)->priv->heap )

//#define mnemosine_addr(mn, h) ( (mn)->heap + (h) )

static unsigned char g_mnemosine_static_heap[MNEMOSINE_STATIC_HEAP_SIZE];

static size_t g_mnemosine_static_hhash[MNEMOSINE_STATIC_HEAP_SIZE];

struct mnemosine_priv_data_ctx {
    unsigned char *heap;
    size_t heap_size;
    size_t *hhash;
    size_t hhash_size;
#if !defined(_WIN32) && !defined(MNEMOSINE_NO_PTHREAD)
    pthread_mutex_t lock;
#endif
};

int mnemosine_init(struct mnemosine_ctx *mn, size_t heap_size, const int use_mnheap) {
    int no_error = 0;

    if (mn == NULL || heap_size == 0) {
        return 0;
    }

    mn->priv = (struct mnemosine_priv_data_ctx *) malloc(sizeof(struct mnemosine_priv_data_ctx));

    if (mn->priv == NULL) {
        return 0;
    }

    mn->priv->heap = NULL;
    mn->priv->heap_size = 0;
    mn->priv->hhash = NULL;
    mn->priv->hhash_size = 0;

#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
    if (pthread_mutex_init(&mn->priv->lock, NULL) != 0) {
        goto mnemosine_init_epilogue;
    }

    pthread_mutex_lock(&mn->priv->lock);
#endif


#if !defined(MNEMOSINE_NO_STATIC_HEAP)
    if (use_mnheap) {
        if (heap_size > MNEMOSINE_STATIC_HEAP_SIZE) {
            goto mnemosine_init_epilogue;
        }
        mn->priv->heap = &g_mnemosine_static_heap[0];
        mn->priv->heap_size = heap_size;
        mn->priv->hhash = &g_mnemosine_static_hhash[0];
        mn->priv->hhash_size = heap_size;
        goto mnemosine_init_epilogue;
    }
#else
    if (use_mnheap) {
        goto mnemosize_ini_epilogue;
    }
#endif

    mn->priv->heap = (unsigned char *) malloc(heap_size);

    if (mn->priv->heap == NULL) {
        goto mnemosine_init_epilogue;
    }

    mn->priv->heap_size = heap_size;

    mn->priv->hhash = (size_t *) malloc(heap_size * sizeof(size_t));

    if (mn->priv->hhash == NULL) {
        goto mnemosine_init_epilogue;
    }

    mn->priv->hhash_size = heap_size;

    no_error = 1;

mnemosine_init_epilogue:

    if (no_error == 0) {
        if (mn->priv != NULL && mn->priv->heap != NULL && !use_mnheap) {
            free(mn->priv->heap);
            mn->priv->heap = NULL;
            mn->priv->heap_size = 0;
            free(mn->priv);
            mn->priv = NULL;
        }
    } else {
        memset(mn->priv->heap, 0, mn->priv->heap_size);
        memset(mn->priv->hhash, 0, mn->priv->hhash_size * sizeof(size_t));
    }

#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
    pthread_mutex_unlock(&mn->priv->lock);
#endif

    return no_error;
}

void *mnemosine_malloc(struct mnemosine_ctx *mn, size_t ssize) {
    void *addr;
    unsigned char *hp, *hp_end;
    int segfound;
    ssize_t segsize;

#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
    pthread_mutex_lock(&mn->priv->lock);
#endif

    addr = NULL;

    if (mn == NULL || ssize == 0) {
        goto mnemosine_malloc_epilogue;
    }

    segfound = 0;
    hp = mn->priv->heap;
    hp_end = mn->priv->heap + mn->priv->heap_size;

    while (!segfound && hp != hp_end) {
        segfound = 1;
        addr = hp;

        for (segsize = 0; hp != hp_end && segfound && segsize < ssize; hp++, segsize++) {
            // INFO(Rafael): Verifying if the current free segment fits the wanted size.
            segfound = (mn->priv->hhash[mnemosine_hash(mn, hp)] == 0);
        }

        segfound = (segfound && segsize == ssize);

        if (!segfound && hp != hp_end) {
            // INFO(Rafael): Finding the next effective free segment.
            do {
                hp++;
            } while (hp != hp_end && mn->priv->hhash[mnemosine_hash(mn, hp)] != 0);
        }
    }

    if (segfound) {
        // INFO(Rafael): Doing the segment allocation.
        hp = addr;
        for (segsize = ssize; segsize > 0; segsize--, hp++) {
            // INFO(Rafael): It will mitigate cases when free() is called not from the base address.
            mn->priv->hhash[mnemosine_hash(mn, hp)] = segsize;
        }
    } else {
        addr = NULL;
    }

mnemosine_malloc_epilogue:

    hp = hp_end = NULL;
    segsize = 0;

#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
    pthread_mutex_unlock(&mn->priv->lock);
#endif

    return addr;
}

int mnemosine_free(struct mnemosine_ctx *mn, void *addr) {
    int no_error;
    size_t asize, hash;

#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
    pthread_mutex_lock(&mn->priv->lock);
#endif

    no_error = 0;

    if (addr != NULL && mn != NULL && (unsigned char *)addr >= mn->priv->heap &&
                                      (unsigned char *)addr < (mn->priv->heap + mn->priv->heap_size)) {
        // INFO(Rafael): Freeing the segment.
        hash = mnemosine_hash(mn, addr);
        asize = mn->priv->hhash[hash];
        memset(addr, 0, asize);
        memset(&mn->priv->hhash[hash], 0, asize * sizeof(size_t));
        no_error = 1;
    }

#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
    pthread_mutex_unlock(&mn->priv->lock);
#endif

    return no_error;
}

void mnemosine_finis(struct mnemosine_ctx *mn) {
    if (mn != NULL) {
#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
        // ???(Rafael): This is kind of crazy, maybe it would be better a reference counter or
        //              simply a semaphore instead of a mutex.
        pthread_mutex_lock(&mn->priv->lock);
#endif
        memset(mn->priv->heap, 0, mn->priv->heap_size);
        memset(mn->priv->hhash, 0, mn->priv->hhash_size * sizeof(size_t));

        if (mn->priv->heap != &g_mnemosine_static_heap[0]) {
            free(mn->priv->heap);
        }

        if (mn->priv->hhash != &g_mnemosine_static_hhash[0]) {
            free(mn->priv->hhash);
        }

        mn->priv->heap_size = mn->priv->hhash_size = 0;

#if defined(__unix__) && !defined(MNEMOSINE_NO_PTHREAD)
        pthread_mutex_destroy(&mn->priv->lock);
#endif

        free(mn->priv);
    }
}

#undef mnemosine_hash
