#ifndef SCREEN_H
#define SCREEN_H

#include <X11/Xlib.h>

typedef struct {
	int row;
	int col;
	int visible;
} TextCursor;

typedef struct {
	Display* display;
	Window window;
	GC gc;
	XFontStruct* font;
	int char_width;
	int char_height;
	TextCursor cursor;
} TermScreen;

void screen_init(Display* display, Window window, GC gc, XFontStruct* font);
void screen_clear();
void screen_draw_text(const char* text, int row, int col);
void screen_draw_cursor();
void screen_refresh();

#endif
