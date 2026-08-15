#ifndef PTY_H
#define PTY_H

#include <sys/ioctl.h>
#include <sys/types.h>

pid_t pty_fork(int *master_fd, const struct winsize *ws);

int pty_set_winsize(int master_fd, int rows, int cols);

#endif
