#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

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

void executeInFork(command *const com) {
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

  char **argv = getArgVector(com);
  execvp(argv[0], argv);

  // should not be reached unless an error occurred
  switch (errno) {
    case ENOENT:
      fprintf(stderr, "%s: no such file or directory\n", argv[0]);
      break;
    case EACCES:
      fprintf(stderr, "%s: permission denied\n", argv[0]);
      break;
    default:
      fprintf(stderr, "%s: exec error\n", argv[0]);
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

  executeInFork(parsed_line->pipeline->commands->com);
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
