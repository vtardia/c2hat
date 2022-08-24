#include "uiinput.h"
#include <string.h>
#include "logger/logger.h"

typedef struct {
  WINDOW *handle;
  int height;
} UIInputWinBox;

typedef struct {
  WINDOW *handle;
  int lines;
  int cols;
  int y;           //< current cursor line
  int x;           //< current cursor column
  int maxY;        //< max cursor line
  int maxX;        //< max cursor column
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
#define UIInputWin_locate() { \
  getyx(this.handle, this.y, this.x); \
  getmaxyx(this.handle, this.maxY, this.maxX); \
}

void UIInputWin_render(const UIScreen *screen, size_t height, size_t start) {
  // Input box container: 20% tall, 100% wide, starts at the bottom of the chat box
  if (wrapper.handle == NULL) {
    wrapper.handle = subwin(screen->handle, height, screen->cols, start, 0);
    wborder(wrapper.handle, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');
    wrefresh(wrapper.handle);
  }

  // Input box, within the container
  if (this.handle == NULL) {
    this.handle = subwin(wrapper.handle, (height - 2), (screen->cols - 2), (start + 1), 1);
    wrefresh(this.handle);
  }

  // To be done after each resize
  getmaxyx(this.handle, this.maxY, this.maxX);
}

void UIInputWin_destroy() {
  UIWindow_destroy(this.handle);
  memset(&this, 0, sizeof(UIInputWin));
  UIWindow_destroy(wrapper.handle);
  memset(&wrapper, 0, sizeof(UIInputWinBox));
}

void UIInputWin_getCursor() {
  wcursyncup(this.handle);
}

void UIInputWin_delete() {
  UIInputWin_locate();
  // Delete the character at the current position,
  // move to a new position and update the counters
  if (this.cursor > 0) {
    int newX, newY;
    if (this.x == 0) {
      // Y must be > 0 if we have text (cursor > 0)
      newX = this.maxX - 1;
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
    // UISetInputCounter(this.eom, this.eob);
  }
}

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
      if (this.x < this.maxX && wmove(this.handle, this.y, this.x + wccols) != ERR) {
        this.cursor += wccols;
        this.eom += wccols;
      } else if (wmove(this.handle, this.y + 1, 0) != ERR) {
        this.cursor += wccols;
        this.eom += wccols;
      }
      // Don't update the cursor if you can't move
    }
    wrefresh(this.handle);
    // UISetInputCounter(inputWinEom, inputWinEob);
  }
}

size_t UIInputWin_commit() {
  // Points to the end of every read block
  wchar_t *cur = this.buffer;
  // mvwinnwstr() reads only one line at a time so we need a loop
  for (int i = 0; i < this.maxY; i++) {
    // thisEob - (cur - buffer) = remaning available unread bytes in the buffer
    int read = mvwinnwstr(this.handle, i, 0, cur, (this.eob - (cur - this.buffer)));
    if (read != ERR) {
      cur += read;
    }
  }
  // Once the input message is collected, clean the window
  wmove(this.handle, 0, 0);
  wclear(this.handle);
  wrefresh(this.handle);
  this.eom = 0;
  // ...and return it to the caller
  if ((cur - this.buffer) > 0) {
    return wcslen(this.buffer) + 1;
  }
  return 0; // Nothing to read
}

void UIInputWin_reset() {
  wmove(this.handle, 0, 0);
  wclear(this.handle);
  wrefresh(this.handle);
  this.cursor = 0;
  this.eom = 0;
  memset(this.buffer, 0, this.length * sizeof(wchar_t));
}

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
  if (this.eob > (this.maxX * this.maxY)) {
    this.eob = this.maxX * this.maxY;
  }
}

void UIInputWin_getCount(int *current, int *max) {
  *current = this.eom;
  *max = this.eob;
}

void MoveLeft() {
  // Advance the cursor if possible
  if (this.y > 0 && this.x == 0) {
    // The text cursor is at the beginning of line 2+,
    // move at the end of the previous line
    if (wmove(this.handle, this.y - 1, this.maxX - 1) != ERR) {
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
  // UISetInputCounter(this.eom, this.eob);
}

void MoveRight() {
  // Move the cursor back, if possible
  // We can move to the right only of there is already text
  if (this.eom > ((this.y * this.maxX) + this.x)) {
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
      this.cursor -= this.maxX;
    }
  }
}

void MoveDown() {
  // Move the cursor to the line below,
  // but only if there is enough text in the line below
  if (this.y < (this.maxY - 1) && this.eom >= (this.maxX + this.x)) {
    if (wmove(this.handle, this.y + 1, this.x) != ERR) {
      wrefresh(this.handle);
      this.cursor += this.maxX;
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
