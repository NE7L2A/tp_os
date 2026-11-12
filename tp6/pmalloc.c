#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <stdint.h>
#include <errno.h>
#include "pmalloc.h"

static size_t pagesize = 0;

/* Un bloc protege = une entree dans la liste chainee */
struct pbloc {
    void          *base;        /* debut du mmap                */
    size_t         total_size;  /* taille totale (avec guard)   */
    size_t         user_size;   /* taille demandee par l'user   */
    int            guard_mode;  /* GUARD_MPROTECT / GUARD_MUNMAP */
    struct pbloc  *next;
};

static struct pbloc *phead = NULL;

/* -------- Handler SIGSEGV -------- */
static void segv_handler(int sig, siginfo_t *info, void *ctx) {
    (void)sig; (void)ctx; (void)info;
    const char *msg = "[pmalloc] SIGSEGV : tentative de buffer overflow detectee !\n";
    ssize_t ignored = write(2, msg, strlen(msg));
(void)ignored;
    _exit(128 + SIGSEGV);
}

static void install_handler(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = segv_handler;
    sa.sa_flags     = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, NULL);
}

static void init_once(void) {
    if (pagesize == 0) {
        pagesize = (size_t)sysconf(_SC_PAGE_SIZE);
        install_handler();
    }
}

/* -------- pmalloc -------- */
void *pmalloc(size_t size, int guard_mode) {
    init_once();
    if (size == 0) size = 1;

    size_t hdr    = sizeof(struct pbloc);
    size_t needed = hdr + size;

    /* pages utiles = arrondi superieur de needed a la taille de page */
    size_t usable = ((needed + pagesize - 1) / pagesize) * pagesize;
    /* une page de plus pour le guard */
    size_t total  = usable + pagesize;

    void *base = mmap(NULL, total, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (base == MAP_FAILED)
        return NULL;

    /* Le buffer doit finir exactement au debut du guard */
    char *guard = (char *)base + usable;
    char *user  = guard - size;

    /* Metadonnees au debut du mmap */
    struct pbloc *blk = (struct pbloc *)base;
    blk->base       = base;
    blk->total_size = total;
    blk->user_size  = size;
    blk->guard_mode = guard_mode;
    blk->next       = phead;
    phead           = blk;

    /* Mise en place du guard */
    if (guard_mode == GUARD_MUNMAP) {
        if (munmap(guard, pagesize) != 0) {
            munmap(base, total);
            phead = blk->next;
            return NULL;
        }
    } else {
        if (mprotect(guard, pagesize, PROT_NONE) != 0) {
            munmap(base, total);
            phead = blk->next;
            return NULL;
        }
    }

    return user;
}

/* -------- pfree -------- */
static struct pbloc *find_blk(void *ptr) {
    struct pbloc *cur = phead;
    while (cur) {
        size_t usable = cur->total_size - pagesize;
        char  *user   = (char *)cur->base + usable - cur->user_size;
        if (user == (char *)ptr) return cur;
        cur = cur->next;
    }
    return NULL;
}

void pfree(void *ptr) {
    if (!ptr) return;
    init_once();

    struct pbloc *prev = NULL, *cur = phead;
    while (cur) {
        size_t usable = cur->total_size - pagesize;
        char  *user   = (char *)cur->base + usable - cur->user_size;
        if (user == (char *)ptr) break;
        prev = cur;
        cur  = cur->next;
    }
    if (!cur) return;

    if (prev) prev->next = cur->next;
    else      phead      = cur->next;

    void  *base   = cur->base;
    size_t total  = cur->total_size;
    size_t usable = total - pagesize;

    if (cur->guard_mode == GUARD_MUNMAP) {
        /* guard deja unmapped : on unmap juste les pages utiles */
        munmap(base, usable);
    } else {
        /* guard etait PROT_NONE : on unmap tout */
        munmap(base, total);
    }
}

/* -------- prealloc -------- */
void *prealloc(void *ptr, size_t size) {
    if (!ptr) return pmalloc(size, GUARD_MPROTECT);
    if (size == 0) { pfree(ptr); return NULL; }

    init_once();
    struct pbloc *blk = find_blk(ptr);
    if (!blk) return NULL;

    if (size <= blk->user_size) return ptr;   /* assez grand */

    void *new_ptr = pmalloc(size, blk->guard_mode);
    if (!new_ptr) return NULL;

    memcpy(new_ptr, ptr, blk->user_size);
    pfree(ptr);
    return new_ptr;
}
