#include <math.h>
#include "screen.h" 
#include "string.h"
#include <stdlib.h>
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

    screen.nrows = SCREEN_HEIGHT / screen.char_height;
    screen.ncols = SCREEN_WIDTH / screen.char_width;

    screen.buf_text = malloc(screen.nrows * sizeof(text_t*));
    for (int i = 0; i < screen.nrows; ++i) {
        screen.buf_text[i] = malloc(screen.ncols * sizeof(text_t));
        memset(screen.buf_text[i], ' ', screen.ncols);
    }    
}

void screen_put_char(text_t c, int row, int col) {
    int _row = row;
    int _col = col;
    if (_row < 0) {
        _row = 0;
    } else if (_row > screen.nrows - 1) {
        _row = screen.nrows - 1;
    }
    if (_col < 0) {
        _col = 0;
    } else if (_col > screen.ncols - 1) {
        _col = screen.ncols - 1;
    }
    screen.buf_text[_row][_col] = c;   
}

text_t* screen_get_char(int row, int col) {
    int _row = row;
    int _col = col;
    if (_row < 0) {
        _row = 0;
    } else if (_row > screen.nrows - 1) {
        _row = screen.nrows - 1;
    }
    if (_col < 0) {
        _col = 0;
    } else if (_col > screen.ncols - 1) {
        _col = screen.ncols - 1;
    }
    return &screen.buf_text[_row][_col];
}

void screen_clear() {
	XSetForeground(screen.display, screen.gc, 0x001e1e1e);
	XFillRectangle(screen.display, screen.window, screen.gc, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void screen_put_text(const char* text, int row, int col) {
    if (row < screen.nrows) {
        int text_len = strlen(text);
        for (int i = 0; i < text_len; ++i) {
            int _col = col + i;
            if (_col < screen.ncols) {
                screen_put_char(text[i], row, _col);
            }
        }
    }
}

void screen_draw_block_text(int row, int col, int nrows, int ncols) {
    for (int i = 0; i < nrows; ++i) {
        int _row = row + i;
        text_t *text = calloc(ncols + 1, sizeof(text_t));
        for (int j = 0; j < ncols; ++j) {
            int _col = col + j;
            if (_row < screen.nrows && _col < screen.ncols) {
                text[j] = screen.buf_text[_row][_col]; 
            }
        }
        text[ncols] = '\0';
        screen_draw_text(text, _row, col);
        free(text);
    }
}

// handy function to redraw full screen, for test only
void screen_draw_all_text() {
    screen_draw_block_text(0, 0, screen.nrows, screen.ncols);
}

// draw char from buffer
void screen_draw_char(int row, int col, long color) {
	int x = col * screen.char_width;
	int y = row * screen.char_height + screen.font->ascent;
	XSetForeground(screen.display, screen.gc, color);
    XDrawString(screen.display, screen.window, screen.gc, x, y, screen_get_char(row, col), 1);
}

void screen_draw_text(const char* text, int row, int col) {
	int x = col * screen.char_width;
	// need to add extra ascent to get to the baseline position
	int y = row * screen.char_height + screen.font->ascent;
	XSetForeground(screen.display, screen.gc, 0x00FFFFFF);
	XDrawString(screen.display, screen.window, screen.gc, x, y, text, strlen(text));
}

void screen_greet() {
	screen_put_text("Hello world!", 0, 1);
	screen_put_text("Hello world!", 20, 10);
}

// TODO you will need to swap fg bg color for the character covered by the cursor
// cursor position needs to be pixel position
// curosr position has to be stored
// to prevent cursor from blocking the text, you need to know what text is under the cursor and invert the color of those text so that it will be visible. Once cursor leave the text, invert the text back
// try array of strings to store lines
void screen_draw_block_cursor(int row, int col) {
	int x = col * screen.char_width;
	int y = row * screen.char_height;
	XSetForeground(screen.display, screen.gc, 0x00FFFFFF);
	XFillRectangle(screen.display, screen.window, screen.gc, x, y, screen.char_width, screen.char_height);	
    screen_draw_char(row, col, 0x00000000);
}

void move_cursor(TextCursor *cursor, int num_cols_shift, int num_rows_shift, int direction, int horizontal) {
	// first remove previous cursor
	remove_cursor(cursor);
    // immediately redraw previous char
    screen_draw_char(cursor->row, cursor->col, 0x00FFFFFF);
    if (horizontal == 1) {
		if (direction == 1) {
			cursor->col = fmin(cursor->col + num_cols_shift, screen.ncols - 1);
		} else if (direction == -1) {
			cursor->col = fmax(cursor->col - num_cols_shift, 0);
		}
    } else {
		if (direction == 1) {
			cursor->row = fmin(cursor->row + num_rows_shift, screen.nrows - 1);
		} else if (direction == -1) {
		    cursor->row = fmax(cursor->row - num_rows_shift, 0);
		}
    }
	screen_draw_block_cursor(cursor->row, cursor->col);
}

// TODO: make it true insert (e.g. shift chars to right before inserting new char)
void insert_at_cursor(TextCursor *cursor, text_t c) {
	screen_put_char(c, cursor->row, cursor->col);
	screen_draw_char(cursor->row, cursor->col, 0x00000000);
}

void remove_cursor(TextCursor *cursor) {
	int x = cursor->col * screen.char_width;
	int y = cursor->row * screen.char_height;
	XSetForeground(screen.display, screen.gc, 0x001e1e1e);
	XFillRectangle(screen.display, screen.window, screen.gc, x, y, screen.char_width, screen.char_height);	
}

void delete_at_cursor(TextCursor *cursor) {
	remove_cursor(cursor);
	screen_put_char(' ', cursor->row, cursor->col);
	screen_draw_block_cursor(cursor->row, cursor->col);
}
