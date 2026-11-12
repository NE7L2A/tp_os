#ifndef TRACKER_H
#define TRACKER_H

#include <stdint.h>

#define PAGEMAP_ENTRY_SIZE 8
#define SOFT_DIRTY_BIT     55

/* Variable globale definie dans tracker.c */
extern long pagesize;

struct region {
    unsigned long start;
    unsigned long end;
    char          perms[8];
    char          name[256];
};

int  read_maps(int pid, struct region **regions, int *count);
int  open_pagemap(int pid);
int  clear_soft_dirty(int pid);
unsigned long scan_region(int fd, unsigned long start, unsigned long end, int print);

#endif
