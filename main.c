#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/cursorfont.h>
#include "scrollbar.h"
#include "screen.h"

Display* main_display = NULL;
Window main_window = 0;
ScrollBar main_scrollbar;
GC main_gc = 0;
XFontStruct* main_font = NULL;

void
create_window()
{
  main_display = XOpenDisplay(NULL);
  if (main_display == NULL) {
    fprintf(stderr, "Error: cannot open display");
  } else {
    /****************
     * window
     ****************/
    // every window needs a root window
    Window root_window = XDefaultRootWindow(main_display);
    // (display, root window, x, y, width, height, border width, border, background)
    main_window = XCreateSimpleWindow(
      main_display, 
      root_window, 
      0, 0, 
      800, 600, 
      0, 
      0, 
      0x001e1e1e);

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
    main_scrollbar.win = XCreateSimpleWindow(
      main_display, 
      main_window, 
      780, 0, 
      20, 600, 
      0,
      0, 
      0x00808080);

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
    XMapWindow(main_display, main_scrollbar.win);
    // flush all data now. Generally not needed after having event loop
    XFlush(main_display);
    screen_init(main_display, main_window, main_gc, main_font);
  }
}

int
main()
{
  create_window();
  XSelectInput(main_display, main_window, ExposureMask);
  for (;;) {
      XEvent e;
      XNextEvent(main_display, &e);

      if (e.type == Expose) {
          screen_draw_text("hello world", 5, 5);
          XFlush(main_display);
      }
  }
}
