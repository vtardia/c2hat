#ifndef UI_INPUT_H
#define UI_INPUT_H
  #include <ncurses.h>
  #include "uiterm.h"

  enum keys {
    kKeyEnter = 10,
    kKeyBackspace = 8,
    kKeyESC = 27,
    kKeyDel = 127,
    kKeyEOT = 4 // Ctrl+D = end of input
  };

  void UIInputWin_render(const UIScreen *screen, size_t height, size_t start);
  void UIInputWin_destroy();

  /// Takes possession of the cursor
  void UIInputWin_getCursor();

  /// Deletes a character
  void UIInputWin_delete();

  /// Moves the cursor within the window, if there is text
  void UIInputWin_moveCursor(wint_t ch);

  /// Adds a character at the current cursor position
  void UIInputWin_addChar(wint_t ch);

  /// Initialises the input window before reading characters
  void UIInputWin_init(wchar_t *buffer, size_t length);

  /// Reads the content of the input buffer
  size_t UIInputWin_commit();

  /// Resets the content and position of the input window
  void UIInputWin_reset();

  void UIInputWin_getCount(int *current, int *max);
#endif
