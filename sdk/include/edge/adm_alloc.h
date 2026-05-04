#ifndef _ADM_ALLOC_H_
#define _ADM_ALLOC_H_

#include <stddef.h>

#include "string.h"

void*
adm_malloc(size_t);
void
adm_free(void*);
void*
adm_realloc(void*, size_t);
void*
adm_memalign(size_t, size_t);
void*
adm_valloc(size_t);
void*
adm_pvalloc(size_t);
void*
adm_calloc(size_t, size_t);
void
adm_cfree(void*);
// int adm_malloc_trim(size_t);
// size_t
// adm_malloc_usable_size(void*);
// void
// adm_malloc_stats(void);
// int
// adm_mallopt(int, int);
// struct mallinfo
// adm_mallinfo(void);

#endif
