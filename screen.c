#include "screen.h"
#include "string.h"
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

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

  screen.view_rows = SCREEN_HEIGHT / screen.char_height;
  screen.view_cols = SCREEN_WIDTH / screen.char_width;
  screen.buf_rows = screen.view_rows;
  screen.buf_cols = screen.view_cols;

  screen.buf_text = malloc(screen.buf_rows * sizeof(text_t *));
  for (int i = 0; i < screen.buf_rows; ++i) {
    screen.buf_text[i] = malloc(screen.buf_cols * sizeof(text_t));
    memset(screen.buf_text[i], ' ', screen.buf_cols);
  }
}

// for debug
void print_screen_buf_text(int view) {
  int nrows = screen.buf_rows;
  if (view) {
    nrows = screen.view_rows;
  }
  for (int i = 0; i < nrows; i++) {
    printf("%s\n", screen.buf_text[i]);
  }
}

void screen_put_char(text_t c, int row, int col) {
  int _row = row;
  int _col = col;
  if (_row < 0) {
    _row = 0;
  } else if (_row > screen.view_rows - 1) {
    _row = screen.view_rows - 1;
  }
  if (_col < 0) {
    _col = 0;
  } else if (_col > screen.view_cols - 1) {
    _col = screen.view_cols - 1;
  }
  screen.buf_text[_row][_col] = c;
}

text_t *screen_get_char(int row, int col) {
  int _row = row;
  int _col = col;
  if (_row < 0) {
    _row = 0;
  } else if (_row > screen.view_rows - 1) {
    _row = screen.view_rows - 1;
  }
  if (_col < 0) {
    _col = 0;
  } else if (_col > screen.view_cols - 1) {
    _col = screen.view_cols - 1;
  }
  return &screen.buf_text[_row][_col];
}

void screen_clear() {
  XSetForeground(screen.display, screen.gc, BG_COLOR);
  XFillRectangle(screen.display, screen.window, screen.gc, 0, 0, SCREEN_WIDTH,
                 SCREEN_HEIGHT);
}

void screen_put_text(const char *text, int row, int col) {
  if (row < screen.view_rows) {
    int text_len = strlen(text);
    for (int i = 0; i < text_len; ++i) {
      int _col = col + i;
      if (_col < screen.view_cols) {
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
      if (_row < screen.view_rows && _col < screen.view_cols) {
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
  screen_draw_block_text(0, 0, screen.view_rows, screen.view_cols);
}

// draw char from buffer
void screen_draw_char(int row, int col, long color) {
  int x = col * screen.char_width;
  int y = row * screen.char_height + screen.font->ascent;
  XSetForeground(screen.display, screen.gc, color);
  XDrawString(screen.display, screen.window, screen.gc, x, y,
              screen_get_char(row, col), 1);
}

void screen_draw_text(const char *text, int row, int col) {
  int x = col * screen.char_width;
  // need to add extra ascent to get to the baseline position
  int y = row * screen.char_height + screen.font->ascent;
  XSetForeground(screen.display, screen.gc, FG_COLOR);
  XDrawString(screen.display, screen.window, screen.gc, x, y, text,
              strlen(text));
}

void screen_greet() {
  screen_put_text("Hello world!", 0, 1);
  screen_put_text("Hello world!", 20, 10);
}

// TODO you will need to swap fg bg color for the character covered by the
// cursor cursor position needs to be pixel position curosr position has to be
// stored to prevent cursor from blocking the text, you need to know what text
// is under the cursor and invert the color of those text so that it will be
// visible. Once cursor leave the text, invert the text back try array of
// strings to store lines
void screen_draw_block_cursor(int row, int col) {
  int x = col * screen.char_width;
  int y = row * screen.char_height;
  XSetForeground(screen.display, screen.gc, FG_COLOR);
  XFillRectangle(screen.display, screen.window, screen.gc, x, y,
                 screen.char_width, screen.char_height);
  screen_draw_char(row, col, 0x00000000);
}

void move_cursor(TextCursor *cursor, int num_cols_shift, int num_rows_shift,
                 int direction, int horizontal) {
  // first remove previous cursor
  remove_cursor(cursor);
  // immediately redraw previous char
  screen_draw_char(cursor->row, cursor->col, FG_COLOR);
  if (horizontal == 1) {
    if (direction == 1) {
      cursor->col = fmin(cursor->col + num_cols_shift, screen.view_cols - 1);
    } else if (direction == -1) {
      cursor->col = fmax(cursor->col - num_cols_shift, 0);
    }
  } else {
    if (direction == 1) {
      cursor->row = fmin(cursor->row + num_rows_shift, screen.view_rows - 1);
    } else if (direction == -1) {
      cursor->row = fmax(cursor->row - num_rows_shift, 0);
    }
  }
  screen_draw_block_cursor(cursor->row, cursor->col);
}

// TODO: make it true insert (e.g. shift chars to right before inserting new
// char)
void insert_at_cursor(TextCursor *cursor, text_t c) {
  screen_put_char(c, cursor->row, cursor->col);
  screen_draw_char(cursor->row, cursor->col, BG_COLOR);
}

void remove_cursor(TextCursor *cursor) {
  int x = cursor->col * screen.char_width;
  int y = cursor->row * screen.char_height;
  XSetForeground(screen.display, screen.gc, BG_COLOR);
  XFillRectangle(screen.display, screen.window, screen.gc, x, y,
                 screen.char_width, screen.char_height);
}

void delete_at_cursor(TextCursor *cursor) {
  remove_cursor(cursor);
  screen_put_char(' ', cursor->row, cursor->col);
  screen_draw_block_cursor(cursor->row, cursor->col);
}

// escape-sequence parser state;
typedef enum {
  ESC_NONE,
  ESC_START,
  ESC_CSI,
  ESC_OSC,
  ESC_OSC_ESC,
} EscState;
static EscState esc_state = ESC_NONE;

void screen_feed(TextCursor *cursor, const char *buf, int len) {
  if (cursor == NULL || buf == NULL || len <= 0) {
    return;
  }
  for (int i = 0; i < len; ++i) {
    unsigned char c = (unsigned char)buf[i];
    // consume and discard escape sequences before normal text handling
    if (esc_state != ESC_NONE) {
      if (esc_state == ESC_START) {
        if (c == '[') {
          esc_state = ESC_CSI;
        } else if (c == ']') {
          esc_state = ESC_OSC;
        } else if (c < 0x20 || c > 0x2f) {
          // intermediates (0x20-0x2f) extend the sequence, a final byte ends it
          esc_state = ESC_NONE;
        }
      } else if (esc_state == ESC_CSI) {
        if (c >= 0x40 && c <= 0x7e) {
          esc_state = ESC_NONE;
        }
      } else if (esc_state == ESC_OSC) {
        if (c == 0x07) {
          esc_state = ESC_NONE;
        } else if (c == 0x1b) {
          esc_state = ESC_OSC_ESC;
        }
      } else if (esc_state == ESC_OSC_ESC) {
        esc_state = ESC_NONE;
      }
      continue;
    }
    if (c == 0x1b) {
      esc_state = ESC_START;
      continue;
    }
    if (c == '\r') {
      cursor->col = 0;
    } else if (c == '\n') {
      if (cursor->row < screen.view_rows - 1) {
        cursor->row += 1;
      }
      cursor->col = 0;
    } else if (c == '\b') {
      if (cursor->col > 0) {
        cursor->col -= 1;
        screen_put_char(' ', cursor->row, cursor->col);
      }
    } else if (c == '\t') {
      int spaces = 4 - (cursor->col % 4);
      for (int s = 0; s < spaces; ++s) {
        if (cursor->col < screen.view_cols) {
          screen_put_char(' ', cursor->row, cursor->col);
        }
        if (cursor->col < screen.view_cols - 1) {
          cursor->col += 1;
        }
      }
    } else if (c >= 32) {
      screen_put_char((text_t)c, cursor->row, cursor->col);
      if (cursor->col < screen.view_cols - 1) {
        cursor->col += 1;
      }
    }
  }
}

int update_screen_size(TextCursor *cursor, int *out_rows, int *out_cols) {
  XWindowAttributes attrs;
  if (!XGetWindowAttributes(screen.display, screen.window, &attrs)) {
    return 0;
  }
  int view_rows = attrs.height / screen.char_height;
  int view_cols = attrs.width / screen.char_width;
  if (view_rows < 1) {
    view_rows = 1;
  }
  if (view_cols < 1) {
    view_cols = 1;
  }
  if (view_rows == screen.view_rows && view_cols == screen.view_cols) {
    return 0;
  }

  if (view_cols > screen.buf_cols) {
    for (int i = 0; i < screen.buf_rows; ++i) {
      screen.buf_text[i] =
          realloc(screen.buf_text[i], view_cols * sizeof(text_t));
      memset(&screen.buf_text[i][screen.buf_cols], ' ',
             view_cols - screen.buf_cols);
    }
    screen.buf_cols = view_cols;
  }
  if (view_rows > screen.buf_rows) {
    screen.buf_text = realloc(screen.buf_text, view_rows * sizeof(text_t *));
    for (int i = screen.buf_rows; i < view_rows; ++i) {
      screen.buf_text[i] = malloc(screen.buf_cols * sizeof(text_t));
      memset(screen.buf_text[i], ' ', screen.buf_cols);
    }
    screen.buf_rows = view_rows;
  }

  screen.view_rows = view_rows;
  screen.view_cols = view_cols;
  if (cursor != NULL) {
    if (cursor->row > view_rows - 1) {
      cursor->row = view_rows - 1;
    }
    if (cursor->col > view_cols - 1) {
      cursor->col = view_cols - 1;
    }
  }
  if (out_rows != NULL) {
    *out_rows = view_rows;
  }
  if (out_cols != NULL) {
    *out_cols = view_cols;
  }
  return 1;
}
