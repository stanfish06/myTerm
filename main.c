#include "pty.h"
#include "screen.h"
#include "scrollbar.h"
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <errno.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <unistd.h>

Display *main_display = NULL;
Window main_window = 0;
ScrollBar main_scrollbar;
GC main_gc = 0;
XFontStruct *main_font = NULL;

void create_window() {
  main_display = XOpenDisplay(NULL);
  if (main_display == NULL) {
    fprintf(stderr, "Error: cannot open display");
  } else {
    /****************
     * window
     ****************/
    // every window needs a root window
    Window root_window = XDefaultRootWindow(main_display);
    // (display, root window, x, y, width, height, border width, border,
    // background)
    main_window =
        XCreateSimpleWindow(main_display, root_window, 0, 0, SCREEN_WIDTH,
                            SCREEN_HEIGHT, 0, 0, BG_COLOR);

    /***************
     * make cursor
     ****************/
    Cursor cursor;
    cursor = XCreateFontCursor(main_display, XC_trek);
    if (cursor == None) {
      fprintf(stderr, "Error: cannot create cursor");
    }
    // add cursor to window
    XDefineCursor(main_display, main_window, cursor);

    /**************
     * scroll bar
     **************/
    // TODO: need to dynamically adjust it
    /* main_scrollbar.win = XCreateSimpleWindow(
      main_display, main_window, 780, 0, 20, SCREEN_HEIGHT, 0, 0, 0x00808080);
    */

    /**************
     * text
     **************/
    main_font = XLoadQueryFont(main_display, "fixed");
    main_gc = XCreateGC(main_display, main_window, 0, NULL);
    XSetFont(main_display, main_gc, main_font->fid);

    /***************
     * display
     ****************/
    // basically show the windows
    XMapWindow(main_display, main_window);
    // XMapWindow(main_display, main_scrollbar.win);
    // flush all data now. Generally not needed after having event loop
    XFlush(main_display);
    screen_init(main_display, main_window, main_gc, main_font);
  }
}

int main() {
  create_window();
  XSelectInput(main_display, main_window,
               ExposureMask | KeyPressMask | StructureNotifyMask);
  TextCursor cursor;
  cursor.row = 0;
  cursor.col = 0;
  cursor.visible = 1;

  struct winsize ws;
  ws.ws_row = SCREEN_HEIGHT / (main_font->ascent + main_font->descent);
  ws.ws_col = SCREEN_WIDTH / (main_font->max_bounds.width);
  ws.ws_xpixel = SCREEN_WIDTH;
  ws.ws_ypixel = SCREEN_HEIGHT;
  int pty_fd = -1;
  if (pty_fork(&pty_fd, &ws) < 0) {
    fprintf(stderr, "Error: failed to start PTY\n");
    return 1;
  }

  int xfd = ConnectionNumber(main_display);
  for (;;) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(xfd, &rfds);
    FD_SET(pty_fd, &rfds);
    int maxfd = (xfd > pty_fd) ? xfd : pty_fd;

    int rc = select(maxfd + 1, &rfds, NULL, NULL, NULL);
    if (rc < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("select");
      break;
    }

    int needs_redraw = 0;
    if (FD_ISSET(pty_fd, &rfds)) {
      char buf[4096];
      int n = (int)read(pty_fd, buf, sizeof(buf));
      if (n > 0) {
        screen_feed(&cursor, buf, n);
        needs_redraw = 1;
      } else if (n == 0) {
        break;
      } else if (errno != EINTR && errno != EAGAIN) {
        perror("read pty");
        break;
      }
    }

    if (FD_ISSET(xfd, &rfds)) {
      while (XPending(main_display)) {
        XEvent e;
        XNextEvent(main_display, &e);
        if (e.type == Expose) {
          needs_redraw = 1;
        } else if (e.type == ConfigureNotify) {
          int vrows, vcols;
          if (update_screen_size(&cursor, &vrows, &vcols)) {
            pty_set_winsize(pty_fd, vrows, vcols);
            needs_redraw = 1;
          }
        } else if (e.type == KeyPress) {
          char buf[32] = {0};
          KeySym keysym;
          int count = XLookupString(&e.xkey, buf, sizeof(buf), &keysym, NULL);
          if (keysym == XK_Return) {
            char cr = '\r';
            write(pty_fd, &cr, 1);
          } else if (keysym == XK_BackSpace) {
            char bs = 0x7f;
            write(pty_fd, &bs, 1);
          } else if (count > 0) {
            write(pty_fd, buf, count);
          }
        }
      }
    }

    print_screen_buf_text(1);

    if (needs_redraw) {
      screen_draw_all_text();
      screen_draw_block_cursor(cursor.row, cursor.col);
      XFlush(main_display);
    }
  }
}
