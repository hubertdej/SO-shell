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

void closeFileDescriptor(int fd) {
  if (close(fd) == -1) {
    perror("close() failed");
    exit(EXIT_FAILURE);
  }
}

void moveFileDescriptor(int from, int to) {
  if (from == to) {
    return;
  }
  if (dup2(from, to) == -1) {
    perror("dup2() failed");
    exit(EXIT_FAILURE);
  }
  closeFileDescriptor(from);
}

void runCommand(command *const com, int read_fd, int pipe_fds[2]) {
  // It is crucial to flush the output stream before calling fork() and exec()
  // so that the buffered characters appear BEFORE whatever the child process outputs.
  fflush(stdout);

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork() failed\n");
    exit(EXIT_FAILURE);
  }

  if (pid != 0) {
    return;
  }

  moveFileDescriptor(read_fd, STDIN_FILENO);
  if (pipe_fds != NULL) {
    moveFileDescriptor(pipe_fds[1], STDOUT_FILENO);
    closeFileDescriptor(pipe_fds[0]);
  }

  char *command_name = com->args->arg;
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

bool isEmptyPipeline(pipeline *p) {
  if (p == NULL || p->commands == NULL) {
    return true;
  }
  if (p->commands->com == NULL && p->commands->next == p->commands) {
    // pipeline consists of exactly one NULL command (empty pipeline); corresponds to "; ;"
    return true;
  }
  return false;
}

// Removes empty pipelines from the list.
// Returns number of pipelines in a resulting list.
// Returns -1 on detecting an invalid pipeline.
int removeEmptyPipelines(pipelineseq **pipelines) {
  if (*pipelines == NULL) {
    return -1;
  }

  int num_pipelines = 0;

  // Count pipelines.
  pipelineseq *curr = *pipelines;
  do {
    ++num_pipelines;
    curr = curr->next;
  } while (curr != *pipelines);

  // Find a first non-empty pipeline.
  for (int i = 0; i < num_pipelines; ++i) {
    if (!isEmptyPipeline(curr->pipeline)) {
      break;
    }
    curr = curr->next;
  }

  if (isEmptyPipeline(curr->pipeline)) {
    // All pipelines are empty.
    *pipelines = NULL;
    return 0;
  }

  // Set the beginning of the list as the first non-empty pipeline.
  *pipelines = curr;

  // Remove all empty pipelines.
  pipelineseq *prev = curr;
  curr = curr->next;
  for (int i = num_pipelines; i >= 1; --i) {
    if (isEmptyPipeline(curr->pipeline)) {
      prev->next = curr = curr->next;
      --num_pipelines;
      continue;
    }
    prev = prev->next;
    curr = curr->next;
  }

  // Check validity.
  curr = *pipelines;
  for (int i = 0; i < num_pipelines; ++i) {
    commandseq *c = curr->pipeline->commands;
    do {
      if (c->com == NULL) {
        // Found a NULL command in a non-empty pipeline; corresponds to "| |".
        *pipelines = NULL;
        return -1;
      }
      c = c->next;
    } while (c != curr->pipeline->commands);
  }

  return num_pipelines;
}

void runPipeline(pipeline *pl) {
  int num_commands = 0;
  commandseq *c = pl->commands;
  do {
    ++num_commands;
    c = c->next;
  } while (c != pl->commands);

  if (num_commands == 1 && c->com->redirs == NULL) {
    char *command_name = c->com->args->arg;
    builtin_ptr builtin = getBuiltin(command_name);
    if (builtin != NULL) {
      if (builtin(getArgVector(c->com)) == BUILTIN_ERROR) {
        fprintf(stderr, "Builtin %s error.\n", command_name);
      }
      return;
    }
  }

  int read_fd = dup(STDIN_FILENO);
  if (read_fd == -1) {
    perror("dup() failed");
    exit(EXIT_FAILURE);
  }

  for (int i = 0; i < num_commands - 1; ++i) {
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
      perror("pipe() failed");
      exit(EXIT_FAILURE);
    }
    runCommand(c->com, read_fd, pipe_fds);
    closeFileDescriptor(read_fd);
    closeFileDescriptor(pipe_fds[1]);
    read_fd = pipe_fds[0];
    c = c->next;
  }
  runCommand(c->com, read_fd, NULL);
  closeFileDescriptor(read_fd);

  while (wait(NULL) != (pid_t)-1) {
    continue;
  }
  if (errno != ECHILD) {
    perror("wait() failed");
    exit(EXIT_FAILURE);
  }
}

void handleLine() {
  errno = 0;
  char *line = getLine();
  if (errno == EIO) {
    fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
    return;
  }
  if (line == NULL) {
    exit(EXIT_SUCCESS);
  }

  pipelineseq *pipelines = parseline(line);
  int count = removeEmptyPipelines(&pipelines);
  if (count == -1) {
    fprintf(stderr, "%s\n", SYNTAX_ERROR_STR);
    return;
  }

  pipelineseq *p = pipelines;
  for (int i = 0; i < count; ++i) {
    runPipeline(p->pipeline);
    p = p->next;
  }
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
