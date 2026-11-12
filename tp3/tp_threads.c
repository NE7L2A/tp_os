#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>

/* Flag partagé : vaut 1 quand le thread est "suspendu" via SIGUSR1 */
static volatile sig_atomic_t suspended = 0;

/* Handlers installés au niveau processus (visibles par tous les threads) */
static void handler(int sig) {
    if (sig == SIGUSR1) {
        /* On se met en pause tant que personne ne nous envoie SIGUSR2 */
        suspended = 1;
        sigset_t empty;
        sigemptyset(&empty);
        while (suspended) {
            sigsuspend(&empty);   /* débloqué dès qu'un signal arrive */
        }
    } else if (sig == SIGUSR2) {
        /* Reprise : on lève le flag */
        suspended = 0;
    }
}

static pthread_t tidA, tidB;

static void *worker_A(void *arg) {
    (void)arg;
    while (1) {
        printf("[A] tick (tid=%lu)\n", (unsigned long)pthread_self());
        fflush(stdout);
        sleep(3);
    }
    return NULL;
}

static void *worker_B(void *arg) {
    (void)arg;
    while (1) {
        printf("[B] tick (tid=%lu)\n", (unsigned long)pthread_self());
        fflush(stdout);
        sleep(3);
    }
    return NULL;
}

static void *scheduler(void *arg) {
    (void)arg;
    printf("[SCHED] attente 10s avant debut du scheduling...\n");
    fflush(stdout);
    sleep(10);

    int turn = 0;
    while (1) {
        if (turn == 0) {
            printf("[SCHED] suspend A, relance B\n");
            pthread_kill(tidA, SIGUSR1);   /* A se suspend */
            pthread_kill(tidB, SIGUSR2);   /* B reprend   */
        } else {
            printf("[SCHED] suspend B, relance A\n");
            pthread_kill(tidB, SIGUSR1);
            pthread_kill(tidA, SIGUSR2);
        }
        fflush(stdout);
        turn = !turn;
        sleep(5);
    }
    return NULL;
}

int main(void) {
    /* Installation les handlers une seule fois, avant tout pthread_create */
    struct sigaction sa;
    sa.sa_handler = handler;
    sa.sa_flags   = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    pthread_create(&tidA, NULL, worker_A, NULL);
    pthread_create(&tidB, NULL, worker_B, NULL);

    pthread_t sched;
    pthread_create(&sched, NULL, scheduler, NULL);

    pthread_join(sched, NULL);   /* on ne sort jamais, mais évite un warning */
    return 0;
}
