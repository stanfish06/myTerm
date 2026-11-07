#include "screen.h"
#include "string.h"
#include <X11/Xlib.h>
#include <X11/cursorfont.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define UTF_SIZ 4
#define ESC_BUF_SIZ (128 * UTF_SIZ)
#define ESC_ARG_SIZ 16
#define ESC_ARG_MAX 65535 // cap for a single CSI parameter
#define TAB_WIDTH 8       // fixed tab stops every N columns

static TermScreen screen;

// allocate an XftColor from a 0x00RRGGBB value
static void alloc_color(unsigned long rgb, XftColor *out) {
  int scr = DefaultScreen(screen.display);
  // XRenderColor channels are 16-bit; 0xab -> 0xabab
  XRenderColor c = {
      .red = ((rgb >> 16) & 0xff) * 0x101,
      .green = ((rgb >> 8) & 0xff) * 0x101,
      .blue = (rgb & 0xff) * 0x101,
      .alpha = 0xffff,
  };
  if (!XftColorAllocValue(screen.display, DefaultVisual(screen.display, scr),
                          DefaultColormap(screen.display, scr), &c, out)) {
    fprintf(stderr, "Error: cannot allocate color 0x%06lx\n", rgb);
    exit(1);
  }
}

void screen_init(Display *display, Window window, GC gc, XftFont *font) {
  screen.display = display;
  screen.window = window;
  screen.gc = gc;
  screen.font = font;

  int scr = DefaultScreen(display);
  screen.draw = XftDrawCreate(display, window, DefaultVisual(display, scr),
                              DefaultColormap(display, scr));
  alloc_color(FG_COLOR, &screen.fg);
  alloc_color(0x000000, &screen.cursor_fg);

  // cell width from one glyph's advance; max_advance_width can be wider than
  // the real cell for fonts with a few oversized glyphs
  XGlyphInfo ext;
  XftTextExtents8(display, font, (const FcChar8 *)"M", 1, &ext);
  screen.char_width = ext.xOff;
  screen.char_height = font->ascent + font->descent;

  screen.cursor.row = 0;
  screen.cursor.col = 0;
  screen.cursor.visible = 1;

  screen.view_rows = SCREEN_HEIGHT / screen.char_height;
  screen.view_cols = SCREEN_WIDTH / screen.char_width;
  screen.buf_rows = screen.view_rows;
  screen.buf_cols = screen.view_cols;
  screen.scroll_top = 0;
  screen.scroll_bot = screen.view_rows - 1;

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

// draw char from buffer; glyph only, the cell background is left as is
void screen_draw_char(int row, int col, XftColor *color) {
  int x = col * screen.char_width;
  int y = row * screen.char_height + screen.font->ascent;
  XftDrawString8(screen.draw, color, screen.font, x, y,
                 screen_get_char(row, col), 1);
}

void screen_draw_text(const char *text, int row, int col) {
  int x = col * screen.char_width;
  // need to add extra ascent to get to the baseline position
  int y = row * screen.char_height + screen.font->ascent;
  int len = strlen(text);
  // Xft draws glyphs only, so clear the cells first (XDrawImageString did both)
  XSetForeground(screen.display, screen.gc, BG_COLOR);
  XFillRectangle(screen.display, screen.window, screen.gc, x,
                 row * screen.char_height, len * screen.char_width,
                 screen.char_height);
  // bytes map to Latin-1 glyphs, same as the core font path
  XftDrawString8(screen.draw, &screen.fg, screen.font, x, y,
                 (const FcChar8 *)text, len);
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
  screen_draw_char(row, col, &screen.cursor_fg);
}

void move_cursor(TextCursor *cursor, int num_cols_shift, int num_rows_shift,
                 int direction, int horizontal) {
  // first remove previous cursor
  remove_cursor(cursor);
  // immediately redraw previous char
  screen_draw_char(cursor->row, cursor->col, &screen.fg);
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

typedef struct {
  char buf[ESC_BUF_SIZ]; // own storage so a sequence can span pty reads
  size_t len;
  char priv; // private marker ('?', '>', '<', '=') or 0
  int arg[ESC_ARG_SIZ];
  int narg;
  char mode[2];
} CSIEscape;
static CSIEscape csiescape;

typedef struct {
  char *buf;
  size_t len;
} STREscape;

typedef enum {
  ESC_NONE,
  ESC_START,
  ESC_INTER, // ESC + intermediate (e.g. ESC ( B), waiting for the final byte
  ESC_CSI,
  ESC_OSC,
  ESC_OSC_ESC,
} EscState;
static EscState esc_state = ESC_NONE;

// set after printing in the last column; the next printable char wraps first
static int wrap_next = 0;
// cursor saved by ESC 7, restored by ESC 8
static TextCursor saved_cursor = {0, 0, 1};
// DECCKM (CSI ?1h/l): cursor keys send SS3 (ESC O x) instead of CSI (ESC [ x)
static int app_cursor_keys = 0;

int screen_app_cursor_keys() { return app_cursor_keys; }

// blank columns [from, to) on a row
static void erase_cells(int row, int from, int to) {
  if (from < 0)
    from = 0;
  if (to > screen.view_cols)
    to = screen.view_cols;
  for (int i = from; i < to; i++)
    screen.buf_text[row][i] = ' ';
}

// move the cursor, clamped to the visible screen; cancels a pending wrap
static void move_to(TextCursor *cursor, int row, int col) {
  if (row < 0)
    row = 0;
  else if (row > screen.view_rows - 1)
    row = screen.view_rows - 1;
  if (col < 0)
    col = 0;
  else if (col > screen.view_cols - 1)
    col = screen.view_cols - 1;
  cursor->row = row;
  cursor->col = col;
  wrap_next = 0;
}

// rotate rows [top, bot] up by n; the n rows entering at bot are blanked
static void scroll_up(int top, int bot, int n) {
  if (n > bot - top + 1)
    n = bot - top + 1;
  for (int k = 0; k < n; k++) {
    text_t *line = screen.buf_text[top];
    memmove(&screen.buf_text[top], &screen.buf_text[top + 1],
            (bot - top) * sizeof(text_t *));
    screen.buf_text[bot] = line;
    memset(line, ' ', screen.buf_cols);
  }
}

// rotate rows [top, bot] down by n; the n rows entering at top are blanked
static void scroll_down(int top, int bot, int n) {
  if (n > bot - top + 1)
    n = bot - top + 1;
  for (int k = 0; k < n; k++) {
    text_t *line = screen.buf_text[bot];
    memmove(&screen.buf_text[top + 1], &screen.buf_text[top],
            (bot - top) * sizeof(text_t *));
    screen.buf_text[top] = line;
    memset(line, ' ', screen.buf_cols);
  }
}

// LF/IND: down one row, scrolling the region when on its bottom margin
static void linefeed(TextCursor *cursor) {
  if (cursor->row == screen.scroll_bot)
    scroll_up(screen.scroll_top, screen.scroll_bot, 1);
  else
    move_to(cursor, cursor->row + 1, cursor->col);
  wrap_next = 0;
}

// RI: up one row, scrolling the region down when on its top margin
static void reverse_index(TextCursor *cursor) {
  if (cursor->row == screen.scroll_top)
    scroll_down(screen.scroll_top, screen.scroll_bot, 1);
  else
    move_to(cursor, cursor->row - 1, cursor->col);
  wrap_next = 0;
}

// ICH: shift the rest of the line right by n and blank the gap
static void insert_char_at_cursor(TextCursor *cursor, int n) {
  int cols = screen.view_cols;
  // clamp so dst never passes the end of the line
  if (n > cols - cursor->col)
    n = cols - cursor->col;
  int src = cursor->col;
  int dst = src + n;
  text_t *line = screen.buf_text[cursor->row];
  memmove(&line[dst], &line[src], (cols - dst) * sizeof(*line));
  erase_cells(cursor->row, src, dst);
}

// DCH: shift the rest of the line left by n and blank the end
static void delete_char_at_cursor(TextCursor *cursor, int n) {
  int cols = screen.view_cols;
  if (n > cols - cursor->col)
    n = cols - cursor->col;
  int dst = cursor->col;
  int src = dst + n;
  text_t *line = screen.buf_text[cursor->row];
  memmove(&line[dst], &line[src], (cols - src) * sizeof(*line));
  erase_cells(cursor->row, cols - n, cols);
}

void parse_csi_escape() {
  char *p = csiescape.buf, *np;
  long int v;
  int sep = ';';
  csiescape.narg = 0;
  // 0x3c-0x3f as the first byte marks a private sequence
  if (*p >= '<' && *p <= '?') {
    csiescape.priv = *p;
    p++;
  }
  csiescape.buf[csiescape.len] = '\0';
  while (p < csiescape.buf + csiescape.len) {
    np = NULL;
    v = strtol(p, &np, 10);
    if (np == p)
      v = 0;
    // clamp so handlers can add args to row/col without overflow
    if (v < 0 || v > ESC_ARG_MAX)
      v = ESC_ARG_MAX;
    csiescape.arg[csiescape.narg++] = v;
    p = np;
    if (sep == ';' && *p == ':')
      sep = ':';
    if (*p != sep || csiescape.narg == ESC_ARG_SIZ)
      break;
    p++;
  }
  csiescape.mode[0] = *p++;
  csiescape.mode[1] = (p < csiescape.buf + csiescape.len) ? *p : '\0';
}

void handle_csi_escape(TextCursor *cursor) {
  int *arg = csiescape.arg;
  int rows = screen.view_rows;
  int cols = screen.view_cols;
  int row = cursor->row;
  int col = cursor->col;

  // private sequences: only DEC modes via ESC[?Nh / ESC[?Nl are handled
  if (csiescape.priv) {
    char m = csiescape.mode[0];
    if (csiescape.priv == '?' && (m == 'h' || m == 'l')) {
      for (int i = 0; i < csiescape.narg; i++) {
        if (arg[i] == 1) // DECCKM
          app_cursor_keys = (m == 'h');
        // other DEC modes (?25 cursor visibility, ?1049 alt screen, ...) are
        // ignored for now
      }
    }
    return;
  }
  // every command except SGR cancels a pending wrap
  if (csiescape.mode[0] != 'm')
    wrap_next = 0;

  switch (csiescape.mode[0]) {
  case '@': // ICH: insert n blanks
    DEFAULT(arg[0], 1);
    insert_char_at_cursor(cursor, arg[0]);
    break;
  case 'A': // CUU: up n
    DEFAULT(arg[0], 1);
    move_to(cursor, row - arg[0], col);
    break;
  case 'B': // CUD: down n
  case 'e': // VPR
    DEFAULT(arg[0], 1);
    move_to(cursor, row + arg[0], col);
    break;
  case 'C': // CUF: right n
  case 'a': // HPR
    DEFAULT(arg[0], 1);
    move_to(cursor, row, col + arg[0]);
    break;
  case 'D': // CUB: left n
    DEFAULT(arg[0], 1);
    move_to(cursor, row, col - arg[0]);
    break;
  case 'E': // CNL: down n, column 1
    DEFAULT(arg[0], 1);
    move_to(cursor, row + arg[0], 0);
    break;
  case 'F': // CPL: up n, column 1
    DEFAULT(arg[0], 1);
    move_to(cursor, row - arg[0], 0);
    break;
  case 'G': // CHA: column n
  case '`': // HPA
    DEFAULT(arg[0], 1);
    move_to(cursor, row, arg[0] - 1);
    break;
  case 'd': // VPA: row n
    DEFAULT(arg[0], 1);
    move_to(cursor, arg[0] - 1, col);
    break;
  case 'H': // CUP: row;col
  case 'f': // HVP
    DEFAULT(arg[0], 1);
    DEFAULT(arg[1], 1);
    move_to(cursor, arg[0] - 1, arg[1] - 1);
    break;
  case 'J': // ED: 0 cursor to end, 1 start to cursor, 2/3 whole screen
    switch (arg[0]) {
    case 0:
      erase_cells(row, col, cols);
      for (int r = row + 1; r < rows; r++)
        erase_cells(r, 0, cols);
      break;
    case 1:
      for (int r = 0; r < row; r++)
        erase_cells(r, 0, cols);
      erase_cells(row, 0, col + 1);
      break;
    case 2:
    case 3: // 3 also clears scrollback, which myTerm doesn't have
      for (int r = 0; r < rows; r++)
        erase_cells(r, 0, cols);
      break;
    }
    break;
  case 'K': // EL: 0 cursor to end, 1 start to cursor, 2 whole line
    switch (arg[0]) {
    case 0:
      erase_cells(row, col, cols);
      break;
    case 1:
      erase_cells(row, 0, col + 1);
      break;
    case 2:
      erase_cells(row, 0, cols);
      break;
    }
    break;
  case 'X': // ECH: blank n chars from the cursor, no shift
    DEFAULT(arg[0], 1);
    erase_cells(row, col, col + arg[0]);
    break;
  case 'P': // DCH: delete n chars
    DEFAULT(arg[0], 1);
    delete_char_at_cursor(cursor, arg[0]);
    break;
  case 'L': // IL: insert n lines at the cursor, only inside the scroll region
    DEFAULT(arg[0], 1);
    if (row >= screen.scroll_top && row <= screen.scroll_bot) {
      scroll_down(row, screen.scroll_bot, arg[0]);
      move_to(cursor, row, 0);
    }
    break;
  case 'M': // DL: delete n lines at the cursor, only inside the scroll region
    DEFAULT(arg[0], 1);
    if (row >= screen.scroll_top && row <= screen.scroll_bot) {
      scroll_up(row, screen.scroll_bot, arg[0]);
      move_to(cursor, row, 0);
    }
    break;
  case 'S': // SU: scroll region up n
    DEFAULT(arg[0], 1);
    scroll_up(screen.scroll_top, screen.scroll_bot, arg[0]);
    break;
  case 'T': // SD: scroll region down n
    DEFAULT(arg[0], 1);
    scroll_down(screen.scroll_top, screen.scroll_bot, arg[0]);
    break;
  case 'r': { // DECSTBM: scroll region top;bottom, then home the cursor
    DEFAULT(arg[0], 1);
    DEFAULT(arg[1], rows);
    int top = arg[0] - 1;
    int bot = (arg[1] > rows ? rows : arg[1]) - 1;
    // a region needs at least two rows; otherwise ignore the request
    if (top < bot) {
      screen.scroll_top = top;
      screen.scroll_bot = bot;
      move_to(cursor, 0, 0);
    }
    break;
  }
  default: // SGR (m) and the rest: ignored until cells carry attributes
    break;
  }
}

// two-byte escapes: ESC <final>
static void handle_esc(TextCursor *cursor, unsigned char c) {
  switch (c) {
  case 'D': // IND: line feed
    linefeed(cursor);
    break;
  case 'E': // NEL: carriage return + line feed
    move_to(cursor, cursor->row, 0);
    linefeed(cursor);
    break;
  case 'M': // RI: reverse line feed
    reverse_index(cursor);
    break;
  case '7': // DECSC: save cursor
    saved_cursor = *cursor;
    break;
  case '8': // DECRC: restore cursor
    move_to(cursor, saved_cursor.row, saved_cursor.col);
    break;
  default: // ESC =, ESC >, ... ignored
    break;
  }
}

// C0 controls; these run immediately, even inside an escape sequence
static void handle_control(TextCursor *cursor, unsigned char c) {
  switch (c) {
  case 0x1b: // ESC (re)starts a sequence, dropping any partial one
    esc_state = ESC_START;
    break;
  case 0x18: // CAN
  case 0x1a: // SUB
    esc_state = ESC_NONE;
    break;
  case '\r':
    move_to(cursor, cursor->row, 0);
    break;
  case '\n':
  case '\v':
  case '\f':
    linefeed(cursor);
    break;
  case '\b': // non-destructive: move left only
    move_to(cursor, cursor->row, cursor->col - 1);
    break;
  case '\t':
    move_to(cursor, cursor->row, (cursor->col / TAB_WIDTH + 1) * TAB_WIDTH);
    break;
  default: // BEL, SO/SI, DEL, ... ignored
    break;
  }
}

// write a char at the cursor, wrapping first if the last write hit the edge
static void put_printable(TextCursor *cursor, text_t c) {
  if (wrap_next) {
    move_to(cursor, cursor->row, 0);
    linefeed(cursor);
  }
  screen_put_char(c, cursor->row, cursor->col);
  if (cursor->col < screen.view_cols - 1)
    cursor->col += 1;
  else
    wrap_next = 1;
}

void screen_feed(TextCursor *cursor, const char *buf, int len) {
  if (cursor == NULL || buf == NULL || len <= 0) {
    return;
  }
  for (int i = 0; i < len; ++i) {
    unsigned char c = (unsigned char)buf[i];
    // OSC strings swallow everything up to BEL or ESC '\'
    if (esc_state == ESC_OSC) {
      if (c == 0x07) {
        esc_state = ESC_NONE;
      } else if (c == 0x1b) {
        esc_state = ESC_OSC_ESC;
      }
      continue;
    }
    if (esc_state == ESC_OSC_ESC) {
      esc_state = ESC_NONE;
      continue;
    }
    if (c < 0x20 || c == 0x7f) {
      handle_control(cursor, c);
      continue;
    }
    switch (esc_state) {
    case ESC_START:
      if (c == '[') {
        // reset args/priv left over from the previous sequence
        memset(&csiescape, 0, sizeof(csiescape));
        esc_state = ESC_CSI;
      } else if (c == ']') {
        esc_state = ESC_OSC;
      } else if (c >= 0x20 && c <= 0x2f) {
        esc_state = ESC_INTER;
      } else {
        handle_esc(cursor, c);
        esc_state = ESC_NONE;
      }
      break;
    case ESC_INTER:
      // final byte ends it; charset selection (ESC ( B) etc. is ignored
      if (c >= 0x30 && c <= 0x7e) {
        esc_state = ESC_NONE;
      }
      break;
    case ESC_CSI:
      // accumulate params; keep one byte for the '\0' parse_csi_escape adds
      if (csiescape.len < ESC_BUF_SIZ - 1)
        csiescape.buf[csiescape.len++] = c;
      // final byte: parse and dispatch once per sequence
      if (c >= 0x40 && c <= 0x7e) {
        parse_csi_escape();
        handle_csi_escape(cursor);
        esc_state = ESC_NONE;
      }
      break;
    default:
      put_printable(cursor, c);
      break;
    }
  }
}

void screen_view_size(int *rows, int *cols) {
  *rows = screen.view_rows;
  *cols = screen.view_cols;
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
  // reset the scroll region to the full screen, as xterm does on resize
  screen.scroll_top = 0;
  screen.scroll_bot = view_rows - 1;
  wrap_next = 0;
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
