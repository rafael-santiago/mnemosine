/*
 *                          Copyright (C) 2019 by Rafael Santiago
 *
 * Use of this source code is governed by GPL-v2 license that can
 * be found in the COPYING file.
 *
 */
#ifndef MNEMOSINE_MNEMOSINE_H
#define MNEMOSINE_MNEMOSINE_H 1

#include <stdlib.h>

#define mnemosine_size_kb(s) ( (size_t)(s) * ((size_t)1 << 10) )
#define mnemosine_size_mb(s) ( (size_t)(s) * ((size_t)1 << 20) )
#define mnemosine_size_gb(s) ( (size_t)(s) * ((size_t)1 << 30) )

struct mnemosine_ctx {
    struct mnemosine_priv_data_ctx *priv;
};

int mnemosine_init(struct mnemosine_ctx *mn, size_t heap_size, const int use_mnheap);

void *mnemosine_malloc(struct mnemosine_ctx *mn, size_t ssize);

int mnemosine_free(struct mnemosine_ctx *mn, void *addr);

void mnemosine_finis(struct mnemosine_ctx *mn);

#endif
