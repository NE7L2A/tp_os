#ifndef PMALLOC_H
#define PMALLOC_H

#include <stddef.h>

#define GUARD_MPROTECT 0   /* page de garde en PROT_NONE */
#define GUARD_MUNMAP   1   /* page de garde unmapped     */

void *pmalloc(size_t size, int guard_mode);
void  pfree(void *ptr);
void *prealloc(void *ptr, size_t size);

#endif
