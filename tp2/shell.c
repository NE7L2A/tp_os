#define _GNU_SOURCE
#include "shell.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>

/* -------- Données globales -------- */
Process processes[MAX_PROC];
int     nb_processes = 0;
EnvVar  env_vars[MAX_ENV];
int     nb_env = 0;
pid_t   foreground_pid = 0;
pid_t   last_suspended_pid = 0;

/* Flag : vrai pendant qu'on attend un fils en avant-plan.
   Le handler SIGCHLD ne doit pas "voler" l'événement dans ce cas. */
volatile sig_atomic_t foreground_running = 0;

/* =========================================================
   Gestion des processus
   ========================================================= */
int add_process(pid_t pid, pid_t ppid, const char *cmd, int bg) {
    if (nb_processes >= MAX_PROC) return -1;
    Process *p = &processes[nb_processes];
    p->pid   = pid;
    p->ppid  = ppid;
    strncpy(p->command, cmd, MAX_CMD - 1);
    p->command[MAX_CMD - 1] = '\0';
    p->state      = ST_RUNNING;
    p->background = bg;
    p->nohup      = 0;
    return nb_processes++;
}

void remove_process(pid_t pid) {
    for (int i = 0; i < nb_processes; i++) {
        if (processes[i].pid == pid) {
            processes[i] = processes[nb_processes - 1];
            nb_processes--;
            return;
        }
    }
}

int find_process(pid_t pid) {
    for (int i = 0; i < nb_processes; i++)
        if (processes[i].pid == pid) return i;
    return -1;
}

/* =========================================================
   Variables d'environnement
   ========================================================= */
int add_env(const char *name, const char *value) {
    for (int i = 0; i < nb_env; i++) {
        if (strcmp(env_vars[i].name, name) == 0) {
            strncpy(env_vars[i].value, value, 511);
            env_vars[i].value[511] = '\0';
            return 0;
        }
    }
    if (nb_env >= MAX_ENV) return -1;
    strncpy(env_vars[nb_env].name, name, 63);
    env_vars[nb_env].name[63] = '\0';
    strncpy(env_vars[nb_env].value, value, 511);
    env_vars[nb_env].value[511] = '\0';
    nb_env++;
    return 0;
}

char *get_env(const char *name) {
    for (int i = 0; i < nb_env; i++)
        if (strcmp(env_vars[i].name, name) == 0)
            return env_vars[i].value;
    return NULL;
}

/* =========================================================
   Recherche d'une commande externe
   ========================================================= */
int find_command(const char *cmd, char *result) {
    /* 1) Chemin absolu */
    if (cmd[0] == '/') {
        if (access(cmd, X_OK) == 0) {
            strcpy(result, cmd);
            return 1;
        }
        return 0;
    }

    char *path = get_env("PATH");

    /* 2) Pas de PATH : on tente ./cmd */
    if (path == NULL) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "./%s", cmd);
        if (access(tmp, X_OK) == 0) {
            strcpy(result, tmp);
            return 1;
        }
        return 0;
    }

    /* 3) Parcours du PATH (séparateur ':') */
    char pathcopy[512];
    strncpy(pathcopy, path, 511);
    pathcopy[511] = '\0';

    char *dir = strtok(pathcopy, ":");
    while (dir != NULL) {
        char tmp[512];
        snprintf(tmp, sizeof(tmp), "%s/%s", dir, cmd);
        if (access(tmp, X_OK) == 0) {
            strcpy(result, tmp);
            return 1;
        }
        dir = strtok(NULL, ":");
    }
    return 0;
}

/* =========================================================
   Exécution d'une commande externe
   ========================================================= */
void execute_external(char **args, int bg) {
    char path[512];
    if (!find_command(args[0], path)) {
        printf("shell: commande introuvable : %s\n", args[0]);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Le fils doit remettre les signaux par défaut,
           sinon il hériterait des handlers du shell
           (et donc ne mourrait jamais sur Ctrl-C). */
        signal(SIGINT,  SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);
        execv(path, args);
        perror("execv");
        exit(1);
    }

    /* Père */
    add_process(pid, getpid(), args[0], bg);

    if (!bg) {
        foreground_pid     = pid;
        foreground_running = 1;
        int status;
        waitpid(pid, &status, WUNTRACED);
        foreground_running = 0;
        foreground_pid     = 0;

        int i = find_process(pid);
        if (i >= 0) {
            if (WIFSTOPPED(status)) {
                processes[i].state = ST_STOPPED;
                last_suspended_pid = pid;
                printf("\n[%d] suspendu\n", pid);
            } else {
                remove_process(pid);
            }
        }
    }
    /* Si bg == 1 : on ne fait pas wait, SIGCHLD s'en chargera */
}

/* =========================================================
   Commandes internes
   ========================================================= */
void cmd_ps(void) {
    printf("%-8s %-8s %-6s %-6s %s\n",
           "PID", "PPID", "ETAT", "TYPE", "CMD");
    for (int i = 0; i < nb_processes; i++) {
        const char *etat = (processes[i].state == ST_RUNNING) ? "RUN" :
                           (processes[i].state == ST_STOPPED) ? "STOP" : "TERM";
        const char *type = processes[i].background ? "bg" : "fg";
        printf("%-8d %-8d %-6s %-6s %s\n",
               processes[i].pid, processes[i].ppid,
               etat, type, processes[i].command);
    }
}

void cmd_env(void) {
    for (int i = 0; i < nb_env; i++)
        printf("%s=%s\n", env_vars[i].name, env_vars[i].value);
}

void cmd_export(char **args) {
    if (args[1] == NULL) {
        printf("usage: export NOM=VALEUR\n");
        return;
    }
    char *eq = strchr(args[1], '=');
    if (eq == NULL) {
        printf("usage: export NOM=VALEUR\n");
        return;
    }
    *eq = '\0';                 /* coupe au niveau du '=' */
    add_env(args[1], eq + 1);
}

void cmd_kill(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        printf("usage: kill [-9|-STOP|-CONT] PID\n");
        return;
    }
    int sig;
    if      (strcmp(args[1], "-9")    == 0) sig = SIGKILL;
    else if (strcmp(args[1], "-STOP") == 0) sig = SIGSTOP;
    else if (strcmp(args[1], "-CONT") == 0) sig = SIGCONT;
    else { printf("signal inconnu\n"); return; }

    pid_t pid = (pid_t)atoi(args[2]);
    if (find_process(pid) < 0) {
        printf("shell: %d n'est pas un processus du shell\n", pid);
        return;
    }
    kill(pid, sig);
}

void cmd_bg(void) {
    if (last_suspended_pid <= 0) {
        printf("aucun processus suspendu\n");
        return;
    }
    kill(last_suspended_pid, SIGCONT);
    int i = find_process(last_suspended_pid);
    if (i >= 0) {
        processes[i].state      = ST_RUNNING;
        processes[i].background = 1;
    }
    last_suspended_pid = 0;
}

void cmd_exit(void) {
    for (int i = 0; i < nb_processes; i++) {
        if (!processes[i].nohup)
            kill(processes[i].pid, SIGKILL);
    }
    exit(0);
}

/* =========================================================
   Handlers de signaux
   ========================================================= */

/* Ctrl-C : on redirige SIGINT vers le fils en avant-plan.
   Le shell lui-même ne meurt pas. */
void sigint_handler(int sig) {
    (void)sig;
    if (foreground_pid > 0)
        kill(foreground_pid, SIGINT);
}

/* Ctrl-Z : on redirige SIGTSTP vers le fils en avant-plan. */
void sigtstp_handler(int sig) {
    (void)sig;
    if (foreground_pid > 0)
        kill(foreground_pid, SIGTSTP);
}

/* SIGCHLD : un fils a changé d'état (terminé, stoppé, repris).
   Ne s'applique qu'aux processus en arrière-plan, car le
   avant-plan est géré directement par waitpid dans execute_external. */
void sigchld_handler(int sig) {
    (void)sig;
    if (foreground_running) return;   /* laissé à execute_external */

    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED | WCONTINUED)) > 0) {
        int i = find_process(pid);
        if (i < 0) continue;

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            remove_process(pid);
        } else if (WIFSTOPPED(status)) {
            processes[i].state = ST_STOPPED;
            last_suspended_pid = pid;
        } else if (WIFCONTINUED(status)) {
            processes[i].state = ST_RUNNING;
        }
    }
}

/* =========================================================
   Initialisation
   ========================================================= */
void init_shell(void) {
    signal(SIGINT,  sigint_handler);
    signal(SIGTSTP, sigtstp_handler);
    signal(SIGCHLD, sigchld_handler);
}

/* =========================================================
   main
   ========================================================= */
int main(void) {
    init_shell();

    char  line[1024];
    char *args[64];

    while (1) {
        printf("myshell> ");
        fflush(stdout);

        if (fgets(line, sizeof(line), stdin) == NULL) {
            printf("\n");
            break;
        }

        /* Retirer le '\n' */
        line[strcspn(line, "\n")] = '\0';
        if (strlen(line) == 0) continue;

        /* Découpage en tokens */
        int   argc = 0;
        char *p    = line;
        while (*p && argc < 63) {
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '\0') break;
            args[argc++] = p;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) *p++ = '\0';
        }
        args[argc] = NULL;
        if (argc == 0) continue;

        /* Détection du '&' final */
        int bg = 0;
        if (strcmp(args[argc - 1], "&") == 0) {
            bg = 1;
            args[--argc] = NULL;
            if (argc == 0) continue;
        }

        /* Commande interne ou externe */
        if      (strcmp(args[0], "exit")   == 0) cmd_exit();
        else if (strcmp(args[0], "ps")     == 0) cmd_ps();
        else if (strcmp(args[0], "env")    == 0) cmd_env();
        else if (strcmp(args[0], "export") == 0) cmd_export(args);
        else if (strcmp(args[0], "kill")   == 0) cmd_kill(args);
        else if (strcmp(args[0], "bg")     == 0) cmd_bg();
        else if (strcmp(args[0], "nohup")  == 0) {
            if (argc < 2) {
                printf("usage: nohup commande [args...]\n");
            } else {
                execute_external(&args[1], 1);
                if (nb_processes > 0)
                    processes[nb_processes - 1].nohup = 1;
            }
        }
        else {
            execute_external(args, bg);
        }
    }
    return 0;
}
