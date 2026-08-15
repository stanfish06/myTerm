#ifndef SCREEN_H
#define SCREEN_H

#include <X11/Xlib.h>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define BG_COLOR 0x001e1e1e
#define FG_COLOR 0x00FFFFFF

// type for text
typedef unsigned char text_t;

typedef struct {
  int row;
  int col;
  int visible;
} TextCursor;

typedef struct {
  Display *display;
  Window window;
  GC gc;
  XFontStruct *font;
  int char_width;
  int char_height;
  TextCursor cursor;
  text_t **buf_text;
  int nrows;
  int ncols;
} TermScreen;

void screen_init(Display *display, Window window, GC gc, XFontStruct *font);
void screen_greet();
void screen_clear();
void screen_draw_all_text();
void screen_draw_text(const char *text, int row, int col);
void screen_draw_block_cursor(int row, int col);
void move_cursor(TextCursor *cursor, int num_cols_shift, int num_rows_shift,
                 int direction, int horizontal);
void insert_at_cursor(TextCursor *cursor, text_t c);
void remove_cursor(TextCursor *cursor);
void delete_at_cursor(TextCursor *cursor);
void screen_feed(TextCursor *cursor, const char *buf, int len);
void screen_refresh();
void update_screen_size();

#endif
