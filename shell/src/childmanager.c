#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define MAX_CHILDREN 100

typedef struct {
  pid_t pid;
  int status;
} child;

volatile int num_fg_running = 0;
volatile int num_bg_finished = 0;

pid_t fgchildren[MAX_CHILDREN];
child bgchildren[MAX_CHILDREN];

void registerForegroundChild(pid_t pid) {
  fgchildren[num_fg_running++] = pid;
}

bool tryRemoveForegroundChild(pid_t pid) {
  for (int i = 0; i < num_fg_running; ++i) {
    if (pid == fgchildren[i]) {
      fgchildren[i] = fgchildren[--num_fg_running];
      return true;
    }
  }
  return false;
}

void saveBackgroundChildInfo(pid_t pid, int status) {
  bgchildren[num_bg_finished].pid = pid;
  bgchildren[num_bg_finished].status = status;
  ++num_bg_finished;
}

void sigchldHandler(int signum) {
  int saved_errno = errno;
  pid_t pid;
  int status;
  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (!tryRemoveForegroundChild(pid)) {
      saveBackgroundChildInfo(pid, status);
    }
  }
  if (pid == -1 && errno != ECHILD) {
    perror("waitpid() failed");
    exit(EXIT_FAILURE);
  }
  errno = saved_errno;
}

void blockChildSignal() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, NULL);
}

void unblockChildSignal() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

void waitForForegroundChildren() {
  sigset_t mask;
  sigprocmask(0, NULL, &mask);
  sigdelset(&mask, SIGCHLD);
  while (num_fg_running > 0) {
    sigsuspend(&mask);
  }
}

void printBackgroundChildrenInfo() {
  blockChildSignal();
  for (int i = 0; i < num_bg_finished; ++i) {
    child *c = &bgchildren[i];
    if (WIFSIGNALED(c->status)) {
      printf("Background process %d terminated. (killed by signal %d)\n", c->pid, WTERMSIG(c->status));
    } else {
      printf("Background process %d terminated. (exited with status %d)\n", c->pid, c->status);
    }
  }
  num_bg_finished = 0;
  unblockChildSignal();
}
