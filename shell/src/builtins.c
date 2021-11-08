#include "builtins.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

int builtinCd(char *argv[]);
int builtinEcho(char *[]);
int builtinExit(char *argv[]);
int builtinKill(char *argv[]);
int builtinLs(char *argv[]);

builtin_pair builtins_table[] = {
    {"cd", &builtinCd},
    {"exit", &builtinExit},
    {"lcd", &builtinCd},
    {"lecho", &builtinEcho},
    {"lkill", &builtinKill},
    {"lls", &builtinLs},
    {NULL, NULL},
};

builtin_ptr getBuiltin(char *command_name) {
  for (builtin_pair *bp = builtins_table; bp->name != NULL; ++bp) {
    if (strcmp(command_name, bp->name) == 0) {
      return bp->fun;
    }
  }
  return NULL;
}

int builtinCd(char *argv[]) {
  if (argv[2] != NULL) {
    return BUILTIN_ERROR;
  }
  char *path;
  if ((path = argv[1]) == NULL && (path = getenv("HOME")) == NULL) {
    return BUILTIN_ERROR;
  }
  if (chdir(path) == -1) {
    return BUILTIN_ERROR;
  }
  return 0;
}

int builtinEcho(char *argv[]) {
  if (argv[1] != NULL) {
    printf("%s", argv[1]);
  }
  for (int i = 2; argv[i] != NULL; ++i) {
    printf(" %s", argv[i]);
  }
  printf("\n");
  return 0;
}

int builtinExit(char *argv[]) {
  exit(EXIT_SUCCESS);
}

int builtinKill(char *argv[]) {
  if (argv[1] == NULL) {
    return BUILTIN_ERROR;
  }

  char *pid_string = argv[1];
  long signal = SIGTERM;

  if (argv[1][0] == '-') {
    pid_string = argv[2];
    errno = 0;
    char *end_ptr;
    signal = strtol(argv[1] + 1, &end_ptr, 10);
    if (errno != 0 || *end_ptr != '\0' || signal > INT_MAX) {
      return BUILTIN_ERROR;
    }
  }

  errno = 0;
  char *end_ptr;
  long pid = strtol(pid_string, &end_ptr, 10);
  if (errno != 0 || *end_ptr != '\0' || (pid_t)pid != pid) {
    return BUILTIN_ERROR;
  }

  if (kill(pid, signal) == -1) {
    return BUILTIN_ERROR;
  }
  return 0;
}

int builtinLs(char *argv[]) {
  char *name = argv[1];
  if (name == NULL) {
    name = ".";
  }

  DIR *dir = opendir(name);
  if (dir == NULL) {
    return BUILTIN_ERROR;
  }

  errno = 0;
  struct dirent *dir_entry;
  while ((dir_entry = readdir(dir)) != NULL) {
    if (dir_entry->d_name[0] != '.') {
      printf("%s\n", dir_entry->d_name);
    }
  }
  if (errno != 0 || closedir(dir) == -1) {
    return BUILTIN_ERROR;
  }
  return 0;
}
