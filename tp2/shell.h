#ifndef SHELL_H
#define SHELL_H

#include <sys/types.h>

#define MAX_PROC 64
#define MAX_ENV  32
#define MAX_CMD  256

/* États d'un processus */
#define ST_RUNNING     0
#define ST_STOPPED     1
#define ST_TERMINATED  2

typedef struct {
    pid_t pid;
    pid_t ppid;
    char  command[MAX_CMD];
    int   state;
    int   background;  /* 0 = fg, 1 = bg */
    int   nohup;       /* 1 = ne pas tuer à la sortie du shell */
} Process;

typedef struct {
    char name[64];
    char value[512];
} EnvVar;

/* Variables globales définies dans shell.c */
extern Process processes[MAX_PROC];
extern int     nb_processes;
extern EnvVar  env_vars[MAX_ENV];
extern int     nb_env;
extern pid_t   foreground_pid;
extern pid_t   last_suspended_pid;

/* Prototypes */
void  init_shell(void);
int   add_process(pid_t pid, pid_t ppid, const char *cmd, int bg);
void  remove_process(pid_t pid);
int   find_process(pid_t pid);

int   add_env(const char *name, const char *value);
char *get_env(const char *name);

int   find_command(const char *cmd, char *result);
void  execute_external(char **args, int bg);

void  cmd_ps(void);
void  cmd_env(void);
void  cmd_export(char **args);
void  cmd_kill(char **args);
void  cmd_bg(void);
void  cmd_exit(void);

#endif
