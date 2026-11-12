#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include <unistd.h>

#define STACK_SIZE (64 * 1024)

/* 3 contextes : le principal (scheduler) et les deux "threads" */
static ucontext_t ctx_main, ctx_A, ctx_B;

/* "Thread" A : imprime et rend la main au scheduler */
static void func_A(void) {
    while (1) {
        printf("[A] tick\n");
        fflush(stdout);
        swapcontext(&ctx_A, &ctx_main);
    }
}

/* "Thread" B : pareil */
static void func_B(void) {
    while (1) {
        printf("[B] tick\n");
        fflush(stdout);
        swapcontext(&ctx_B, &ctx_main);
    }
}

/* Prépare un contexte à partir d'une fonction */
static void setup_context(ucontext_t *ctx, void (*func)(void)) {
    getcontext(ctx);                                 /* init le contexte   */
    ctx->uc_stack.ss_sp   = malloc(STACK_SIZE);      /* pile dédiée        */
    ctx->uc_stack.ss_size = STACK_SIZE;
    ctx->uc_link          = &ctx_main;               /* retour quand fini  */
    makecontext(ctx, func, 0);                       /* point d'entrée     */
}

int main(void) {
    setup_context(&ctx_A, func_A);
    setup_context(&ctx_B, func_B);

    /* Phase 1 : 10s de latence, comme dans la version pthread */
    printf("[SCHED] 10 secondes de latence avant scheduling...\n");
    fflush(stdout);
    sleep(10);

    /* Phase 2 : alternance stricte entre A et B, 5s chacun */
    printf("[SCHED] debut de l'alternance (5s par contexte)\n");
    fflush(stdout);

    while (1) {
        swapcontext(&ctx_main, &ctx_A);   /* exécute A une fois */
        sleep(5);
        swapcontext(&ctx_main, &ctx_B);   /* exécute B une fois */
        sleep(5);
    }
    return 0;
}
