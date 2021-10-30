#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins.h"
#include "config.h"
#include "reader.c"
#include "siparse.h"

char **getArgVector(command *const com) {
  static char *argv[(MAX_LINE_LENGTH + 1) / 2 + 1];
  int count = 0;
  argseq *current = com->args;

  do {
    argv[count++] = current->arg;
    current = current->next;
  } while (current != com->args);
  argv[count] = NULL;

  return argv;
}

void runCommand(command *const com) {
  char *command_name = com->args->arg;

  for (builtin_pair *bp = builtins_table; bp->name != NULL; ++bp) {
    if (strcmp(command_name, bp->name) == 0) {
      if (bp->fun(getArgVector(com)) == BUILTIN_ERROR) {
        fprintf(stderr, "Builtin %s error.\n", command_name);
      }
      return;
    }
  }

  // It is crucial to flush the output stream before calling fork() and exec()
  // so that the buffered characters appear BEFORE whatever the child process outputs.
  fflush(stdout);

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork() failed\n");
    exit(EXIT_FAILURE);
  }

  if (pid != 0) {
    if (waitpid(pid, NULL, 0) == -1) {
      perror("waitpid() failed");
      exit(EXIT_FAILURE);
    }
    return;
  }

  execvp(command_name, getArgVector(com));

  // Should not be reached unless an error occurred.
  switch (errno) {
    case ENOENT:
      fprintf(stderr, "%s: no such file or directory\n", command_name);
      break;
    case EACCES:
      fprintf(stderr, "%s: permission denied\n", command_name);
      break;
    default:
      fprintf(stderr, "%s: exec error\n", command_name);
      break;
  }
  exit(EXEC_FAILURE);
}

void handleLine() {
  char *line = getLine();
  if (errno == EIO) {
    fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
    return;
  }
  if (line == NULL) {
    exit(EXIT_SUCCESS);
  }

  pipelineseq *parsed_line = parseline(line);
  if (parsed_line == NULL) {
    fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
    return;
  }

  if (parsed_line->pipeline == NULL ||
      parsed_line->pipeline->commands == NULL ||
      parsed_line->pipeline->commands->com == NULL) {
    return;
  }

  runCommand(parsed_line->pipeline->commands->com);
}

int main(int argc, char *argv[]) {
  bool is_a_tty = isatty(STDIN_FILENO);

  while (true) {
    if (is_a_tty) {
      printf(PROMPT_STR);
      fflush(stdout);
    }
    handleLine();
  }

  return EXIT_SUCCESS;
}
