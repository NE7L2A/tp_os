#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tracker.h"

#define VARIATION_THRESHOLD 5
#define STABLE_ITERATIONS   3

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <pid>\n", argv[0]);
        return 1;
    }
    int pid = atoi(argv[1]);
    pagesize = sysconf(_SC_PAGE_SIZE);

    struct region *regs;
    int n;
    if (read_maps(pid, &regs, &n) < 0) return 1;

    int fd = open_pagemap(pid);
    if (fd < 0) return 1;

    unsigned long prev = 0;
    int stable = 0;
    int iter = 0;

    while (stable < STABLE_ITERATIONS) {
        iter++;

        /* 1) Marquer toutes les pages comme non sales */
        if (clear_soft_dirty(pid) < 0) break;

        /* 2) Laisser le processus tourner un peu */
        sleep(1);

        /* 3) Scanner les pages sales */
        unsigned long total = 0;
        printf("--- Iteration %d ---\n", iter);
        for (int i = 0; i < n; i++) {
            unsigned long d = scan_region(fd, regs[i].start, regs[i].end, 1);
            total += d;
        }
        printf("Pages sales : %lu\n", total);

        /* 4) Verifier la stabilite */
        long delta = (long)total - (long)prev;
        if (delta < 0) delta = -delta;
        if (iter > 1 && delta <= VARIATION_THRESHOLD)
            stable++;
        else
            stable = 0;

        prev = total;
    }

    printf("=== Stabilisation atteinte apres %d iterations ===\n", iter);

    close(fd);
    free(regs);
    return 0;
}
