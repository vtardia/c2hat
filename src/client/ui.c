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

enum config {
  /// Max message length, in characters, including the NULL terminator
  kMaxMessageLength = 281
};

#define UISetInputCounter() { \
  int current = 0; \
  int max = 0; \
  UIInputWin_getCount(&current, &max);\
  UIStatus_set(kUIStatusPositionRight, "%4d/%d", current, max); \
  UIInputWin_getCursor(); \
}

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

void UIRender() {
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
    UIStatusBar_render(&screen);
    curs_set(kCursorStateNormal);
    UIInputWin_getCursor();
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

  UIRender();
}

/**
 * First responder
 *
 * Manages errors and special characters
 *
 * Returns:
 *
 *  0   on F1/terminate
 *  >0  on new input message to send
 *  -1  on messages available
 *  -2  on resize
 *  -10 on other error
 */
//
int UIInputLoop(wchar_t *buffer, size_t length) {
  memset(buffer, 0, length * sizeof(wchar_t));

  wint_t ch = 0;
  UIInputWin_init(buffer, length);
  UISetInputCounter();
  while (!terminate) {
    int res = wget_wch(screen.handle, &ch);

    if (res == ERR) /* Including EINTR */ {
      if (terminate) {
        Debug("Loop break on terminate");
        break;
      } else if (update) {
        update = false;
        return kUIUpdate;
      // } else if ( resize ) {
      //   return kUIResize;
      } else if (errno == EINTR) {
        // Let event handlers take care of this
        continue;
      } else {
        return -10; // Other error
        // Or display error and continue
      }
    }

    // Exit on F1
    if (ch == KEY_F(1)) {
      Debug("Loop break on F1");
      break;
    }

    // The responsibility here should be of the Input Window
    switch(ch) {

      case kKeyBackspace:
      case kKeyDel:
      case KEY_BACKSPACE:
        Debug("Delete pressed");
        // Delete the character at the current position,
        // move to a new position and update the counters
        UIInputWin_delete();
        UISetInputCounter();
      break;

      case kKeyEnter:
      case kKeyEOT: // Ctrl + D
        Debug("Enter pressed");
        // Read the content of the window up to the given limit
        // and returns it to the caller function
        {
          size_t charRead = UIInputWin_commit();
          if (charRead > 0) return charRead;
        }
      break;

      case kKeyESC:
        if (UIChatWin_getStatus() == kChatWinStatusBrowse) {
          // If in browse mode, exit and go live
          // UISetChatModeLive();
          // UIDrawChatWinContent();
        } else {
          // If in Live mode, cancel any input operation and reset everything
          UIInputWin_reset();
          UISetInputCounter();
        }
      break;

      case KEY_LEFT:
        Debug("Left arrow pressed");
        // Advance the cursor if possible
        UIInputWin_moveCursor(KEY_LEFT);
      break;

      case KEY_RIGHT:
        Debug("Right arrow pressed");
        // Move the cursor back, if possible
        // We can move to the right only of there is already text
        UIInputWin_moveCursor(KEY_RIGHT);
      break;

      case KEY_UP:
        Debug("Up arrow pressed");
        // Move the cursor to the line above, if possible
        UIInputWin_moveCursor(KEY_UP);
      break;

      case KEY_DOWN:
        Debug("Down pressed");
        // Move the cursor to the line below,
        // but only if there is enough text in the line below
        UIInputWin_moveCursor(KEY_DOWN);
      break;

      case KEY_PPAGE: // Page up
        // If Browse mode: display previous page if available
        // If Live mode: set browse mode
      break;

      case KEY_NPAGE: // Page down
        // If Browse mode: display next page if available
      break;

      default:
        Debug("Got input: %c", ch);
        // If we have space, AND the input character is not a control char,
        // add the new character to the message window
        UIInputWin_addChar(ch);
        UISetInputCounter();
      break;
    }
  }
  // An input error happened, cleanup and return error
  memset(buffer, 0, length * sizeof(wchar_t));
  // TODO reset inputwin?
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

  // Create a new format with the app data and source message
  return UIStatus_set(
    kUIStatusPositionLeft,
    "[%s] [%d,%d] %s",
    UIChatWin_getStatus() == kChatWinStatusBrowse ? "B" : "C",
    screen.lines, screen.cols,
    message
  );
}
