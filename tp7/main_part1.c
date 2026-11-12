#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tracker.h"

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

    printf("=== Pages soft-dirty du processus %d ===\n", pid);
    unsigned long total = 0;
    for (int i = 0; i < n; i++) {
        unsigned long d = scan_region(fd, regs[i].start, regs[i].end, 1);
        if (d) {
            printf("  region [%lx-%lx] %s : %lu pages sales\n",
                   regs[i].start, regs[i].end, regs[i].name, d);
        }
        total += d;
    }
    printf("TOTAL : %lu pages sales\n", total);

    close(fd);
    free(regs);
    return 0;
}
