#include "screen.h" 
#include "string.h"
#include <X11/Xlib.h>
#include <X11/cursorfont.h>

static TermScreen screen;

void screen_init(Display *display, Window window, GC gc, XFontStruct *font) {
	screen.display = display;
	screen.window = window;
	screen.gc = gc;
	screen.font = font;
	
	screen.char_width = font->max_bounds.width;
	screen.char_height = font->ascent + font->descent;
	
	screen.cursor.row = 0;
	screen.cursor.col = 0;
	screen.cursor.visible = 1;
}

void screen_clear() {
	XSetForeground(screen.display, screen.gc, 0x001e1e1e);
	XFillRectangle(screen.display, screen.window, screen.gc, 0, 0, 800, 600);
}

void screen_draw_text(const char* text, int row, int col) {
	int x = col * screen.char_width;
	// need to add extra ascent to get to the baseline position
	int y = row * screen.char_height + screen.font->ascent;
	XSetForeground(screen.display, screen.gc, 0x00FFFFFF);
	XDrawString(screen.display, screen.window, screen.gc, x, y, text, strlen(text));
}

// TODO you will need to swap fg bg color for the character covered by the cursor
// cursor position needs to be pixel position
// curosr position has to be stored
void screen_draw_block_cursor(int row, int col) {
	XSetForeground(screen.display, screen.gc, 0x00FFFFFF);
	XFillRectangle(screen.display, screen.window, screen.gc, 0, 0, screen.char_width, screen.char_height);	
}
