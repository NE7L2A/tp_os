#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define N 100
int main(void) {
    char *buf[N];
    for (int i = 0; i < N; i++)
        buf[i] = malloc(4096);

    while (1) {
        for (int i = 0; i < N; i++)
            buf[i][0] = (char)i;   /* ecrit 1 octet -> salit la page */
        sleep(1);
    }
    return 0;
}
