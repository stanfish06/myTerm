#define _GNU_SOURCE

#include "pty.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* termios.h
struct winsize {
  unsigned short ws_row;
  unsigned short ws_col;
  unsigned short ws_xpixel;
  unsigned short ws_ypixel;
};*/

pid_t pty_fork(int *master_fd, const struct winsize *ws) {
  if (master_fd == NULL) {
    errno = EINVAL;
    return -1;
  }

  int mfd = posix_openpt(O_RDWR | O_NOCTTY);
  if (mfd < 0) {
    perror("posix_openpt");
    return -1;
  }
  if (grantpt(mfd) < 0) {
    perror("grantpt");
    close(mfd);
    return -1;
  }
  if (unlockpt(mfd) < 0) {
    perror("unlockpt");
    close(mfd);
    return -1;
  }

  char slavename[128];
  if (ptsname_r(mfd, slavename, sizeof(slavename)) != 0) {
    perror("ptsname_r");
    close(mfd);
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    close(mfd);
    return -1;
  }

  if (pid == 0) {
    if (setsid() < 0) {
      perror("setsid");
      _exit(1);
    }

    int sfd = open(slavename, O_RDWR);
    if (sfd < 0) {
      perror("open slave pty");
      _exit(1);
    }

    if (ioctl(sfd, TIOCSCTTY, 0) < 0) {
      perror("TIOCSCTTY");
      _exit(1);
    }

    if (ws != NULL) {
      if (ioctl(sfd, TIOCSWINSZ, ws) < 0) {
        perror("TIOCSWINSZ");
      }
    }

    if (dup2(sfd, STDIN_FILENO) < 0 || dup2(sfd, STDOUT_FILENO) < 0 ||
        dup2(sfd, STDERR_FILENO) < 0) {
      perror("dup2");
      _exit(1);
    }

    if (sfd > STDERR_FILENO) {
      close(sfd);
    }
    close(mfd);

    const char *shell = getenv("SHELL");
    if (shell == NULL || shell[0] == '\0') {
      shell = "/bin/bash";
    }
    execlp(shell, shell, (char *)NULL);
    perror("execlp");
    _exit(127);
  }

  *master_fd = mfd;
  return pid;
}

int pty_set_winsize(int master_fd, int rows, int cols) {
  struct winsize ws;
  memset(&ws, 0, sizeof(ws));
  ws.ws_row = (unsigned short)rows;
  ws.ws_col = (unsigned short)cols;
  return ioctl(master_fd, TIOCSWINSZ, &ws);
}
