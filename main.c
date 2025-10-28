#include <X11/Xlib.h>
#include <stdio.h>
#include <unistd.h>

void
create_window()
{
  Display* main_display = XOpenDisplay(0);
  if (main_display == NULL) {
    printf("open display failed");
  } else {
    // every window needs a root window
    Window root_window = XDefaultRootWindow(main_display);
    // (display, root window, x, y, width, height, border width, border, background)
    Window main_window = XCreateSimpleWindow(main_display, root_window, 0, 0, 800, 600, 0, 0, 0x001e1e1e);
    // basically show the window
    XMapWindow(main_display, main_window);
    // flush all data now. Generally not needed after having event loop
    XFlush(main_display);
  }
}

int
main()
{
  create_window();
  for (;;) { sleep(1); };
}
