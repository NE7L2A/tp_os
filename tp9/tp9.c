#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>

#define N 100

/* Lit un entier sur un descripteur. Renvoie 1 si lu, 0 si EOF. */
static int read_int(int fd, int *val) {
    ssize_t n = read(fd, val, sizeof(int));
    return n == (ssize_t)sizeof(int);
}

/* Ecrit un entier sur un descripteur. */
static void write_int(int fd, int val) {
    if (write(fd, &val, sizeof(int)) != (ssize_t)sizeof(int)) {
        perror("write");
        _exit(1);
    }
}

int main(void) {
    /* ---------- 1) Redirection de stdout vers resultat.txt ---------- */
    int out_fd = open("resultat.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("open");
        return 1;
    }
    if (dup2(out_fd, STDOUT_FILENO) < 0) {
        perror("dup2");
        return 1;
    }
    close(out_fd);

    /* ---------- 2) Pipe initial : le pere ecrit 2..N dedans ---------- */
    int pipe_in[2];
    if (pipe(pipe_in) < 0) {
        perror("pipe");
        return 1;
    }

    for (int i = 2; i <= N; i++) {
        if (write(pipe_in[1], &i, sizeof(int)) != (ssize_t)sizeof(int)) {
            perror("write");
            return 1;
        }
    }
    close(pipe_in[1]);   /* signal de fin pour le lecteur */

    int current_fd = pipe_in[0];

    /* ---------- 3) Boucle : un fils par nombre premier ---------- */
    while (1) {
        int premier;
        if (!read_int(current_fd, &premier))
            break;                 /* plus rien a lire */

        printf("%d\n", premier);   /* va dans resultat.txt */
        fflush(stdout);            /* important avant fork */

        /* Pipe pour renvoyer le reste au pere */
        int pipe_out[2];
        if (pipe(pipe_out) < 0) {
            perror("pipe");
            return 1;
        }

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            return 1;
        }

        if (pid == 0) {
            /* ---------------- FILS ----------------
             * Lit tout current_fd, filtre les multiples de 'premier',
             * ecrit le reste dans pipe_out[1].
             */
            close(pipe_out[0]);   /* le fils n'ecoute pas */

            int val;
            while (read_int(current_fd, &val)) {
                if (val % premier != 0)
                    write_int(pipe_out[1], val);
            }

            close(current_fd);
            close(pipe_out[1]);
            _exit(0);
        } else {
            /* ---------------- PERE ----------------
             * Ferme ce qu'il n'utilise pas, et lit le nouveau pipe
             * au tour suivant.
             */
            close(pipe_out[1]);        /* le pere n'ecrit pas */
            close(current_fd);         /* il n'ecoute plus l'ancien pipe */
            current_fd = pipe_out[0];  /* lit le nouveau pipe au prochain tour */
        }
    }

    /* ---------- 4) Attendre tous les fils ---------- */
    while (wait(NULL) > 0)
        ;

    return 0;
}
