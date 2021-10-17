#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
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
      printf("%s: no such file or directory\n", argv[0]);
      break;
    case EACCES:
      printf("%s: permission denied\n", argv[0]);
      break;
    default:
      printf("%s: exec error\n", argv[0]);
      break;
  }
  exit(EXEC_FAILURE);
}

void handleLine() {
  printf("%s", PROMPT_STR);
  fflush(stdout);

  char line_buffer[MAX_LINE_LENGTH + 1];

  ssize_t size = read(STDIN_FILENO, line_buffer, MAX_LINE_LENGTH);
  if (size == -1) {
    perror("read() failed");
    exit(EXIT_FAILURE);
  }

  if (size == 0) {
    // EOF
    exit(EXIT_SUCCESS);
  }

  line_buffer[size] = '\0';
  pipelineseq *parsed_line = parseline(line_buffer);
  if (parsed_line == NULL) {
    printf("%s\n", SYNTAX_ERROR_STR);
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
  while (true) {
    handleLine();
  };

  return EXIT_SUCCESS;
}
