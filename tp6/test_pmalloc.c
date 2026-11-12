#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pmalloc.h"

int main(int argc, char **argv) {
    int mode = (argc > 1 && strcmp(argv[1], "munmap") == 0)
               ? GUARD_MUNMAP : GUARD_MPROTECT;

    printf("=== Mode : %s ===\n",
           mode == GUARD_MUNMAP ? "munmap" : "mprotect");

    /* Test 1 : ecriture dans les bornes */
    char *p = pmalloc(100, mode);
    if (!p) { printf("pmalloc a echoue\n"); return 1; }
    printf("pmalloc(100) = %p\n", (void *)p);

    for (int i = 0; i < 100; i++) p[i] = 'A';
    printf("Ecriture dans les bornes : OK\n");

    /* Test 2 : overflow volontaire -> doit segfault */
    printf("Tentative d'overflow (p[100] = 'B')...\n");
    fflush(stdout);
    p[100] = 'B';   /* touche la page de garde -> SIGSEGV */

    printf("Pas de segfault : PROBLEME !\n");
    pfree(p);
    return 0;
}
