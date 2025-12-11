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

void create_window()
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

int main()
{
	create_window();
	XSelectInput(main_display, main_window, ExposureMask | KeyPressMask);
	TextCursor cursor;
	cursor.row     = 0;
	cursor.col     = 0;
	cursor.visible = 1;
    int greeted = 0;
	for (;;) {
		XEvent e;
		XNextEvent(main_display, &e);
        if (!greeted) {
            screen_greet();
            greeted = 1;
        }
        screen_draw_all_text();
		screen_draw_block_cursor(cursor.row, cursor.col);
		if (e.type == KeyPress) {
			char buf[1] = {0};
			KeySym keysym;
			XLookupString(&e.xkey, buf, 1, &keysym, NULL);
			screen_draw_text("key pressed!", 1, 1);
			printf("key %s pressed\n", buf);
			// move cursor
			switch(keysym) {
				case XK_Left:
					move_cursor(&cursor, 1, -1);
					break;
				case XK_Right:
					move_cursor(&cursor, 1, 1);
					break;
			}
		}
		printf("cursor x: %d", cursor.col);
		XFlush(main_display);
	}
}
