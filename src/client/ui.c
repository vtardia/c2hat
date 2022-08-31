#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>
#include <pthread.h>

#include "../c2hat.h"
#include "logger/logger.h"
#include "message/message.h"
#include "ui.h"
#include "uiterm.h"
#include "uichat.h"
#include "uiinput.h"
#include "uistatus.h"
#include "uicolor.h"

/// The main screen
static UIScreen screen = {};

/// Input loop termination flag
static atomic_bool terminate = false;

/// Input loop update-ready flag
static atomic_bool update = false;

/// Input loop resize flag
static atomic_bool resize = false;

enum config {
  /// Max message length, in characters, including the NULL terminator
  kMaxMessageLength = 281
};

#define UISetInputCounter() { \
  int current = 0;                                              \
  int max = 0;                                                  \
  UIInputWin_getCount(&current, &max);                          \
  UIStatus_set(kUIStatusinputCounter, "%4d/%d", current, max);  \
  UIInputWin_getCursor();                                       \
}

#define UISetStatusMode() UIStatus_set(kUIStatusMode, "%c", UIChatWin_getMode() == kChatWinModeBrowse ? 'B' : 'C');

#define UISetStatusSize() UIStatus_set(kUIStatusTerminalSize, "%d,%d", screen.lines, screen.cols);

/**
 * Cleans up the visual environment
 */
void UIClean() {
  // Destroy windows
  UIStatusBar_destroy();
  UIInputWin_destroy();
  UIChatWin_destroy();
  UIScreen_destroy(screen);

  // Close ncurses
  endwin();
}

void UIRender(bool resized) {
  if (resized) {
    UIWindow_reset(screen.handle);
    getmaxyx(screen.handle, screen.lines, screen.cols);
    if (wresize(screen.handle, screen.lines, screen.cols) != OK) {
      Error("Problems while resizing the main screen");
    }
    wclear(screen.handle);
  }

  if (UIScreen_isBigEnough(&screen, kMaxMessageLength)) {
    // Set the sizes of container windows based on the current terminal
    // With narrow terminals we need 4 input lines to fit the whole message
    // so the chat log will be smaller. With wider terminals we can get away
    // with 3 lines for the input and leave the rest for the chat log
    size_t inputWinBoxHeight = (screen.cols < kWideTerminalCols) ? 6  : 5;
    // The upper border requires 1 line
    size_t inputWinBoxStart = screen.lines - (inputWinBoxHeight + 1);
    size_t chatWinBoxHeight = inputWinBoxStart;

    UIChatWin_render(&screen, chatWinBoxHeight, " C2Hat ");

    UIInputWin_render(&screen, inputWinBoxHeight, inputWinBoxStart);
    UIInputWin_reset();

    UIStatusBar_render(&screen);
    UISetStatusMode();
    UISetStatusSize();

    if (UIChatWin_getMode() == kChatWinModeLive) {
      curs_set(kCursorStateNormal);
      UIInputWin_getCursor();
    }
    wnoutrefresh(screen.handle);
    doupdate();
    return;
  }
  UIRender_terminalTooSmall(&screen);
}

/**
 * Initialises NCurses configuration
 */
void UIInit() {
  if (screen.handle != NULL) return; // Already initialised

  // Correctly display Advanced Character Set in UTF-8 environment
  setenv("NCURSES_NO_UTF8_ACS", "0", 1);

  UIScreen_clear();

  if ((screen.handle = initscr()) == NULL) {
    fprintf(
      stderr,
      "❌ Error: Unable to initialise the user interface\n%s\n",
      strerror(errno)
    );
    exit(EXIT_FAILURE);
  }
  getmaxyx(screen.handle, screen.lines, screen.cols);

  UIColor_init();

  // Read input one char at a time,
  // disable line buffering, keeping CTRL as default SIGINT
  cbreak();

  // Capture Fn and other special keys
  keypad(screen.handle, TRUE);

  // Don't automatically echo input, let the program to manage it
  noecho();

  // Normal cursor
  curs_set(kCursorStateNormal);

  // Time for getch() to wait before accepting the ESC key
  // (in milliseconds, default 1000)
  set_escdelay(200);

  // Initialises the random generator
  srand(time(NULL));

  if (!UIChatWin_init()) exit(EXIT_FAILURE);

  UIRender(false);
}

/// First responder
int UIInputLoop(wchar_t *buffer, size_t length, void(*updateHandler)()) {
  memset(buffer, 0, length * sizeof(wchar_t));

  wint_t ch = 0;
  UIInputWin_init(buffer, length);
  UISetInputCounter();
  while (!terminate) {
    int res = wget_wch(screen.handle, &ch);

    if (res == ERR) /* Including EINTR */ {
      if (terminate) {
        break;
      } else if (update) {
        update = false;
        if (UIScreen_isBigEnough(&screen, kMaxMessageLength)) {
          updateHandler();
        }
        continue;
      } else if (resize) {
        resize = false;
        Info("Resize requested (flag)");
        ch = KEY_RESIZE;
      } else if (errno == EINTR) {
        // Let event handlers take care of this
        continue;
      } else {
        return -1; // Other error
      }
    }

    // Exit on F1
    if (ch == KEY_F(1)) {
      break;
    }

    if (ch == KEY_RESIZE) {
      Info("Resize requested (key resize)");
      UIRender(true);
      UISetInputCounter();
      continue;
    }

    // The responsibility here should be of the Input Window
    switch(ch) {

      case kKeyBackspace:
      case kKeyDel:
      case KEY_BACKSPACE:
        // Delete the character at the current position,
        // move to a new position and update the counters
        UIInputWin_delete();
        UISetInputCounter();
      break;

      case kKeyEnter:
      case kKeyEOT: // Ctrl + D
        // Read the content of the window up to the given limit
        // and returns it to the caller function
        {
          size_t charRead = UIInputWin_commit();
          if (charRead > 0) return charRead;
        }
      break;

      case kKeyESC:
        if (UIChatWin_getMode() == kChatWinModeBrowse) {
          // If in browse mode, exit and go live
          UIChatWin_setMode(kChatWinModeLive);
          UISetStatusMode(); // Updates mode on status bar
          UIInputWin_getCursor();
        } else {
          // If in Live mode, cancel any input operation and reset everything
          UIInputWin_reset();
          UISetInputCounter();
        }
      break;

      case KEY_LEFT:
        // Advance the cursor if possible
        UIInputWin_moveCursor(KEY_LEFT);
      break;

      case KEY_RIGHT:
        // Move the cursor back, if possible
        // We can move to the right only of there is already text
        UIInputWin_moveCursor(KEY_RIGHT);
      break;

      case KEY_UP:
        // Move the cursor to the line above, if possible
        UIInputWin_moveCursor(KEY_UP);
      break;

      case KEY_DOWN:
        // Move the cursor to the line below,
        // but only if there is enough text in the line below
        UIInputWin_moveCursor(KEY_DOWN);
      break;

      case KEY_PPAGE: // Page up
        if (UIChatWin_getMode() == kChatWinModeBrowse) {
          // Display the previous screen if available
          UIChatWin_previousPage();
        } else {
          // Set browse mode
          UIChatWin_setMode(kChatWinModeBrowse);
          UISetStatusMode(); // Updates mode on status bar
        }
      break;

      case KEY_NPAGE: // Page down
        if (UIChatWin_getMode() == kChatWinModeBrowse) {
          // Display the next screen if available
          UIChatWin_nextPage();
        }
      break;

      default:
        // If we have space, AND the input character is not a control char,
        // add the new character to the message window
        UIInputWin_addChar(ch);
        UISetInputCounter();
      break;
    }
  }
  // An input error happened, cleanup and return error
  memset(buffer, 0, length * sizeof(wchar_t));
  return kUITerminate;
}

/// Sets termination flag
void UITerminate() {
  terminate = true;
}

/// Sets update notification flag
void UIUpdateChatLog() {
  update = true;
}

/// Sets resize flag
void UIResize() {
  resize = true;
  endwin();
  refresh();
}

/// Adds a message to the chat log queue
void UILogMessage(char *buffer, size_t length) {
  if (Message_getType(buffer) == kMessageTypeQuit) {
    // The server sent a disconnect command
    char *error = "/err You have been disconnected";
    UIChatWin_logMessage(error, strlen(error));
    Info("Session terminated by the server");
    pthread_kill(pthread_self(), SIGTERM);
    return;
  }

  // Other type of message
  UIChatWin_logMessage(buffer, length);
  UIInputWin_getCursor();
}

/// Updates the status bar message
bool UISetStatus(char *format, ...) {
  va_list args;
  va_start(args, format);

  // Parse the source message
  char message[kMaxStatusMessageSize] = {};
  vsnprintf(
    message,
    sizeof(message),
    format,
    args
  );
  va_end(args);
  return UIStatus_set(kUIStatusMessage, "%s", message);
}
