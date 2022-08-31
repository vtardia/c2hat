#include "uiinput.h"
#include <string.h>
#include <wctype.h>

typedef struct {
  WINDOW *handle;
  int lines;
  int cols;
} UIInputWinBox;

typedef struct {
  WINDOW *handle;
  int lines;
  int cols;
  int y;           //< current cursor line
  int x;           //< current cursor column
  int cursor;      //< Variable index pointer that follow the cursor on the window
  int eom;         //< Variable index pointer that point to the end of the typed message
  int eob;         //< Keeps track of character limit for the input message,
                   //  starts at 0 and it's normally messageLength -1
  wchar_t *buffer; //< The buffer that will contain the message
  size_t length;   //< The maximum number of Unicode characters in the buffer
  wchar_t *end;    //< Fixed pointer to the very end of the buffer
} UIInputWin;


/// Input window container, used for sizing and border
static UIInputWinBox wrapper = {};

/// Main input window, used to receive content
static UIInputWin this = {};

// Gets the position of the cursor in the current window
#define UIInputWin_locate() {                   \
  getyx(this.handle, this.y, this.x);           \
  getmaxyx(this.handle, this.lines, this.cols); \
}

void UIInputWin_render(const UIScreen *screen, size_t height, size_t start) {
  // This happens on resize: it's safer and faster to destroy and recreate the window
  if (this.handle != NULL) UIWindow_destroy(this.handle);
  if (wrapper.handle != NULL) UIWindow_destroy(wrapper.handle);

  // Input box container: 20% tall, 100% wide, starts at the bottom of the chat box
  wrapper.handle = derwin(screen->handle, height, screen->cols, start, 0);
  getmaxyx(wrapper.handle, wrapper.lines, wrapper.cols);
  wborder(wrapper.handle, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');

  // Input box, within the container
  this.handle = derwin(wrapper.handle, (wrapper.lines - 2), (wrapper.cols - 2), 1, 1);

  UIInputWin_locate();
  wnoutrefresh(wrapper.handle);
  wnoutrefresh(this.handle);
}

/// Cleanup mamory and resources
void UIInputWin_destroy() {
  UIWindow_destroy(this.handle);
  memset(&this, 0, sizeof(UIInputWin));
  UIWindow_destroy(wrapper.handle);
  memset(&wrapper, 0, sizeof(UIInputWinBox));
}

/// Grab the cursor
void UIInputWin_getCursor() {
  wcursyncup(this.handle);
}

/**
 * Deletes the character at the current position,
 * and move to a new position and update the counters
 */
void UIInputWin_delete() {
  UIInputWin_locate();
  if (this.cursor > 0) {
    int newX, newY;
    if (this.x == 0) {
      // Y must be > 0 if we have text (cursor > 0)
      newX = this.cols - 1;
      newY = this.y - 1;
    } else {
      // X > 0, Y = whatever
      newY = this.y;
      newX = this.x - 1;
    }
    if (mvwdelch(this.handle, newY, newX) != ERR) {
      this.cursor--;
      this.eom--;
      wrefresh(this.handle);
    }
  }
}

/// Adds a character to the input window at the current cursor position
void UIInputWin_addChar(wint_t ch) {
  UIInputWin_locate();
  // If we have space, AND the input character is not a control char,
  // add the new character to the message window
  if (this.cursor < this.eob && !iswcntrl(ch)) {
    // Appending content to the end of the line
    wchar_t wstr[] = { ch, L'\0' };
    int wccols = wcwidth(wstr[0]); // Width of char in columns
    if (this.cursor == this.eom && (waddwstr(this.handle, wstr) != ERR)) {
      this.cursor += wccols;
      this.eom += wccols;
    }
    // Inserting content in the middle of a line
    if (this.cursor < this.eom && (wins_wstr(this.handle, wstr) != ERR)) {
      if (this.x < this.cols && wmove(this.handle, this.y, this.x + wccols) != ERR) {
        this.cursor += wccols;
        this.eom += wccols;
      } else if (wmove(this.handle, this.y + 1, 0) != ERR) {
        this.cursor += wccols;
        this.eom += wccols;
      }
      // Don't update the cursor if you can't move
    }
    wrefresh(this.handle);
  }
}

/**
 * Collects the content of the input window into a message
 * and returns it to the calling function
 */
size_t UIInputWin_commit() {
  if (this.buffer == NULL || this.eom == 0) return 0;

  // Points to the end of every read block
  wchar_t *cur = this.buffer;
  // mvwinnwstr() reads only one line at a time so we need a loop
  for (int i = 0; i < this.lines; i++) {
    // thisEob - (cur - buffer) = remaning available unread bytes in the buffer
    int read = mvwinnwstr(this.handle, i, 0, cur, (this.eob - (cur - this.buffer)));
    if (read != ERR) {
      cur += read;
    }
  }
  int total = this.eom;

  // Once the input message is collected, clean the window
  wmove(this.handle, 0, 0);
  wclear(this.handle);
  wrefresh(this.handle);
  this.eom = 0;

  // ...and return it to the caller
  if (total > 0) {
    return total + 1;
  }
  return 0; // Nothing to read
}

/// Resets the content and cursor position
void UIInputWin_reset() {
  wmove(this.handle, 0, 0);
  wclear(this.handle);
  wrefresh(this.handle);
  this.cursor = 0;
  this.eom = 0;
  memset(this.buffer, 0, this.length * sizeof(wchar_t));
}

/// Prepares the input window for receiving data
void UIInputWin_init(wchar_t *buffer, size_t length) {
  this.cursor = 0;
  this.buffer = buffer;
  this.length = length;
  // Keeps track of character limit and ensures we have
  // the last character as NULL terminator
  this.eob = length -1;

  // Fixed pointer to the very end of the buffer
  this.end = buffer + this.eob;
  // Add a safe NULL terminator at the end of buffer
  *(this.end) = L'\0';

  wmove(this.handle, 0, 0);
  wclear(this.handle);
  wrefresh(this.handle);
  getyx(this.handle, this.y, this.x);
  if (this.eob > (this.cols * this.lines)) {
    this.eob = this.cols * this.lines;
  }
}

/// Gets the current and max length of the input content
void UIInputWin_getCount(int *current, int *max) {
  *current = this.eom;
  *max = this.eob;
}

void MoveLeft() {
  // Advance the cursor if possible
  if (this.y > 0 && this.x == 0) {
    // The text cursor is at the beginning of line 2+,
    // move at the end of the previous line
    if (wmove(this.handle, this.y - 1, this.cols - 1) != ERR) {
      wrefresh(this.handle);
      if (this.cursor > 0) this.cursor--;
    }
  } else if (this.x > 0) {
    // The text cursor is in the middle of a line,
    // just move to the left by one step
    if (wmove(this.handle, this.y, this.x - 1) != ERR) {
      wrefresh(this.handle);
      if (this.cursor > 0) this.cursor--;
    }
  }
}

void MoveRight() {
  // Move the cursor back, if possible
  // We can move to the right only of there is already text
  if (this.eom > ((this.y * this.cols) + this.x)) {
    if (wmove(this.handle, this.y, this.x + 1) != ERR) {
      wrefresh(this.handle);
      this.cursor++;
      // Cursor cannot be greater than the end of message
      if (this.cursor > this.eom) this.cursor = this.eom;
    }
  }
}

void MoveUp() {
  // Move the cursor to the line above, if possible
  if (this.y > 0) {
    if (wmove(this.handle, this.y - 1, this.x) != ERR) {
      wrefresh(this.handle);
      this.cursor -= this.cols;
    }
  }
}

void MoveDown() {
  // Move the cursor to the line below,
  // but only if there is enough text in the line below
  if (this.y < (this.lines - 1) && this.eom >= (this.cols + this.x)) {
    if (wmove(this.handle, this.y + 1, this.x) != ERR) {
      wrefresh(this.handle);
      this.cursor += this.cols;
    }
  }
}

void UIInputWin_moveCursor(wint_t ch) {
  UIInputWin_locate();
  if (ch == KEY_LEFT) return MoveLeft();
  if (ch == KEY_RIGHT) return MoveRight();
  if (ch == KEY_UP) return MoveUp();
  if (ch == KEY_DOWN) return MoveDown();
}
