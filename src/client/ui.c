#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdatomic.h>

#include "../c2hat.h"
#include "logger/logger.h"
#include "ui.h"


/// Handle for the main screen
static WINDOW *screen;

/// Input loop termination flag
static atomic_bool terminate = false;

/// Input loop update-ready flag
static atomic_bool update = false;

/// Available lines in the main screen
static int screenLines = 0;

/// Available columns in the main screen
static int screenCols = 0;

enum ColorPair {
  kColorPairDefault = 0,
  kColorPairCyanOnDefault = 1,
  kColorPairYellowOnDefault = 2,
  kColorPairRedOnDefault = 3,
  kColorPairBlueOnDefault = 4,
  kColorPairMagentaOnDefault = 5,
  kColorPairGreenOnDefault = 6,
  kColorPairWhiteOnBlue = 7,
  kColorPairWhiteOnRed = 8
};

enum CursorState {
  kCursorStateInvisible = 0,
  kCursorStateNormal = 1,
  kCursorStateVeryVisible = 2
};


/**
 * Clears the console
 */
void UIClearScreen() {
  printf("\e[1;1H\e[2J");
}

/**
 * Destroys the given window object
 */
void UIWindow_destroy(WINDOW *win) {
  if (win == NULL) return;
  wborder(win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
  wbkgd(win, COLOR_PAIR(kColorPairDefault));
  wclear(win);
  wrefresh(win);
  delwin(win);
}

/**
 * Cleans up the visual environment
 */
void UIClean() {
  // Destroy windows
  // UIWindow_destroy(statusBarWin);
  // UIWindow_destroy(inputWin);
  // UIWindow_destroy(inputWinBox);
  // UIWindow_destroy(chatWin);
  // UIWindow_destroy(chatWinBox);
  UIWindow_destroy(screen);
  // Close ncurses
  endwin();
  // Free user list and message buffer
  // Hash_free(&users);
  // List_free(&messages);
  // Queue_free(&events);
}

/**
 * Initialises NCurses configuration
 */
void UIInit() {
  if (screen != NULL) return; // Already initialised

  // Correctly display Advanced Character Set in UTF-8 environment
  setenv("NCURSES_NO_UTF8_ACS", "0", 1);

  UIClearScreen();

  if ((screen = initscr()) == NULL) {
    fprintf(
      stderr,
      "❌ Error: Unable to initialise the user interface\n%s\n", strerror(errno)
    );
    exit(EXIT_FAILURE);
  }
  getmaxyx(screen, screenLines, screenCols);

  // Read input one char at a time,
  // disable line buffering, keeping CTRL as default SIGINT
  cbreak();

  // Capture Fn and other special keys
  keypad(screen, TRUE);

  // Don't automatically echo input, let the program to manage it
  noecho();

  // Normal cursor
  curs_set(kCursorStateNormal);

  // Time for getch() to wait before accepting the ESC key
  // (in milliseconds, default 1000)
  set_escdelay(200);

  // Initialises the random generator
  srand(time(NULL));

  // wrefresh(screen);
}

// First responder
// Manages errors and spechal characters
// Returns 0 on F1/terminate
// Returns >0 on message to sent
// Returns -1 on messages available
// Returns -2 on resize
// Returns -X on other error
int UIInputLoop() {
  wint_t ch = 0;
  while (!terminate) {
    int res = wget_wch(screen, &ch);

    if (res == ERR) /* Including EINTR */ {
      if (terminate) {
        Debug("Loop break on terminate");
        break;
      } else if (update) {
        update = false;
        return -1;
      // } else if ( resize ) {
      //   return -2;
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
    Debug("Got input: %c", ch);
  }
  return 0;
}

void UITerminate() {
  terminate = true;
}

void UIUpdateChatLog() {
  update = true;
}

void UILogMessage(char *buffer, size_t length) {
  waddnstr(screen, buffer, length);
  waddstr(screen, "\n");
  // wrefresh(screen);
}
