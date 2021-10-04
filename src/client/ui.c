/*
 * Copyright (C) 2021 Vito Tardia
 */

// TODO: 1) extract window sizes var as static global and
// initialise them within the init routine
// 2) try to extract the message char counter as well

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <wctype.h>

#include "ui.h"
#include "message/message.h"
#include "hash/hash.h"

enum keys {
  kKeyEnter = 10,
  kKeyBackspace = 8,
  kKeyESC = 27,
  kKeyDel = 127,
  kKeyEOT = 4 // Ctrl+D = end of input
};

enum colorPairs {
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

enum users {
  /// Max username length (in characters) excluding the NULL terminator,
  /// ensure this matches with the one in app.h
  kMaxNicknameLength = 15,
  /// Max username size in bytes, for Unicode characters
  kMaxNicknameSize = kMaxNicknameLength * sizeof(wchar_t)
};

enum config {
  /// Max message length, in characters, including the NULL terminator
  kMaxMessageLength = 281,
  /// Min lines to be available in the terminal (eg. 24x80 term has 22 lines available)
  kMinTerminalLines = 22,
  /// Min columns to be available in the terminal
  kMinTerminalCols = 80,
  /// Min columns to be considered for a wide terminal
  kWideTerminalCols = 94
};

static WINDOW *mainWin, *chatWin, *inputWin, *chatWinBox, *inputWinBox, *statusBarWin;
static char currentStatusBarMessage[512] = {0};
static Hash *users = NULL;

void UIColors();
void UIDrawChatWin();
void UIDrawInputWin();
void UIDrawStatusBar();
void UIDrawTermTooSmall();
void UISetInputCounter(int, int);

/**
 * Get the number of available input lines
 * based on the terminal width
 */
int UIGetInputLines() {
  return (COLS < kWideTerminalCols) ? 4  : 3;
}

/**
 * Checks if we have enough space within the terminal
 * We need at least a 24x80 terminal that can host 280
 * characters in the input window
 */
bool UITermIsBigEnough() {
  int lines= UIGetInputLines();
  return (LINES > kMinTerminalLines && COLS > kMinTerminalCols && ((COLS - 1) * lines) >= kMaxMessageLength);
}

/**
 * Initialises NCurses configuration
 */
void UIInit() {
  if (mainWin != NULL) return; // Already initialised

  if ((mainWin = initscr()) == NULL) {
    fprintf(stderr, "Unable to initialise the user interface: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Read input one char at a time,
  // disable line buffering, keeping CTRL as default SIGINT
  cbreak();

  // Capture Fn and other special keys
  keypad(mainWin, TRUE);

  // Don't automatically echo input, let the program to manage it
  noecho();

  // Normal cursor
  curs_set(1);

  // Time for getch() to wait before accepting the ESC key
  // (in milliseconds, default 1000)
  set_escdelay(200);

  // Initialise colors
  UIColors();

  if (UITermIsBigEnough()) {
    UIDrawChatWin();
    UIDrawInputWin();
    UIDrawStatusBar();
  } else {
    UIDrawTermTooSmall();
  }

  // Initialise Users hash
  users = Hash_new();

  // Initialises the random generator
  srand(time(NULL));
}

/**
 * Destroys the given window object
 */
void UIWindow_destroy(WINDOW *win) {
  wborder(win, ' ', ' ', ' ',' ',' ',' ',' ',' ');
  wrefresh(win);
  delwin(win);
}

/**
 * Cleans up the visual environment
 */
void UIClean() {
  // Destroy windows
  UIWindow_destroy(statusBarWin);
  UIWindow_destroy(inputWin);
  UIWindow_destroy(inputWinBox);
  UIWindow_destroy(chatWin);
  UIWindow_destroy(chatWinBox);
  UIWindow_destroy(mainWin);
  // Close ncurses
  endwin();
  // Free user list
  Hash_free(&users);
}

void UIColors() {
  // Init ncurses color engine
  start_color();
  use_default_colors();

  // We have 8 default ANSI colors
  // COLOR_BLACK
  // COLOR_RED
  // COLOR_GREEN
  // COLOR_YELLOW
  // COLOR_BLUE
  // COLOR_MAGENTA
  // COLOR_CYAN
  // COLOR_WHITE
  // Every terminal can display a max of COLORS (0 based until COLORS-1)

  // Initialising color pairs
  // Every terminal can display a max of COLOR_PAIRS (0 based as above)
  // init_pair(#, [text color (default -1)], [background color (default -1])
  // Color pair 0 is the default for the terminal (white on black)
  // and cannot be changed
  init_pair(kColorPairCyanOnDefault, COLOR_CYAN, -1);
  init_pair(kColorPairYellowOnDefault, COLOR_YELLOW, -1);
  init_pair(kColorPairRedOnDefault, COLOR_RED, -1);
  init_pair(kColorPairBlueOnDefault, COLOR_BLUE, -1);
  init_pair(kColorPairMagentaOnDefault, COLOR_MAGENTA, -1);
  init_pair(kColorPairGreenOnDefault, COLOR_GREEN, -1);
  init_pair(kColorPairWhiteOnBlue, COLOR_WHITE, COLOR_BLUE);
  init_pair(kColorPairWhiteOnRed, COLOR_WHITE, COLOR_RED);
}

/**
 * Draws the chat/log window
 */
void UIDrawChatWin() {
  // If the terminal is not wide enough we need a smaller chat and a bigger
  // input window
  int chatWinBoxHeight = (COLS < kWideTerminalCols) ? (LINES - 7) : (LINES - 6);

  // Create the chat window container: 80% tall, 100% wide, starts at top left
  chatWinBox = subwin(mainWin, chatWinBoxHeight, COLS, 0, 0);

  // Add border, just top and bottom to avoid breaking the layout
  // when the user inserts emojis
  // win, left side, right side, top side, bottom side,
  // corners: top left, top right, bottom left, bottom right
  wborder(chatWinBox, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');

  // Draw title
  const char *title = " C2Hat ";
  size_t titleLength = strlen(title);
  mvwaddch(chatWinBox, 0, (COLS/2) - titleLength/2 - 1 , ACS_RTEE);
  mvwaddstr(chatWinBox, 0, (COLS/2) - titleLength/2, title);
  mvwaddch(chatWinBox, 0, (COLS/2) + titleLength/2 + 1, ACS_LTEE);
  wrefresh(chatWinBox);

  // Draw the scrollable chat log box, within the chat window
  chatWin = subwin(chatWinBox, (chatWinBoxHeight - 2), (COLS - 2), 1, 1);
  scrollok(chatWin, TRUE);
  leaveok(chatWin, TRUE);
}

/**
 * Draws the chat input window
 */
void UIDrawInputWin() {
  // Set sizes: ith narrow terminals we need 4 lines to fit the whole message,
  // wider terminals are ok with 3 lines
  int inputWinBoxHeight = (COLS < kWideTerminalCols) ? 6  : 5;
  int inputWinBoxStart = (COLS < kWideTerminalCols) ? (LINES - 7) : (LINES - 6);

  // Input box container: 20% tall, 100% wide, starts at the bottom of the chat box
  inputWinBox = subwin(mainWin, inputWinBoxHeight, COLS, inputWinBoxStart, 0);
  wborder(inputWinBox, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');
  wrefresh(inputWinBox);

  // Input box, within the container
  inputWin = subwin(inputWinBox, (inputWinBoxHeight - 2), (COLS - 2), (inputWinBoxStart + 1), 1);
}

/**
 * Draws the status bar as last line of the screen
 */
void UIDrawStatusBar() {
  // h, w, posY, posX
  statusBarWin = subwin(mainWin, 1, COLS, LINES -1, 0);
  leaveok(statusBarWin, TRUE);
  // Set window default background
  wbkgd(statusBarWin, COLOR_PAIR(kColorPairWhiteOnBlue));
  wrefresh(statusBarWin);
}

/**
 * Refresh the UI and move the cursor
 * to the input window to receive input
 */
void UILoopInit() {
  wrefresh(chatWin);
  wcursyncup(inputWin);
  wrefresh(inputWin);
}

/**
 * Infinite loop that waits for the user input and
 * returns it to the caller
 */
size_t UIGetUserInput(wchar_t *buffer, size_t length) {
  // Keep track of the input window coordinates
  int y, x, maxY, maxX;

  // Contains the last input character
  wint_t ch = 0;

  // Variable pointer that follow the cursor on the window
  int cursor = 0;

  // Variable pointer that point to the end of the typed message
  int eom = 0;

  // Keeps track of character limit
  int eob = length -1;

  // Fixed pointer to the very end of the buffer
  wchar_t *end = buffer + eob;

  // Add a safe NULL terminator at the end of buffer
  *end = 0;

  // Initialise the input window and counters
  wmove(inputWin, 0, 0);
  wclear(inputWin);
  wrefresh(inputWin);
  getmaxyx(inputWin, maxY, maxX);
  if (eob > (maxX * maxY)) {
    eob = maxX * maxY;
  }
  UISetInputCounter(eom, eob);

  // Wait for input
  while (true) {
    // Receives a Unicode character from the user
    int res = get_wch(&ch);
    if (res == ERR) {
      // EINTR means a signal is received, for example resize
      if (errno != EINTR) break;
    }

    // Exit on F1
    if (ch == KEY_F(1)) break;

    // Ignore resize key
    if (ch == KEY_RESIZE) continue;

    // Fetch the current cursor position and window size
    getyx(inputWin, y, x);
    getmaxyx(inputWin, maxY, maxX);

    // Process the input character
    switch(ch) {
      case kKeyBackspace:
      case kKeyDel:
      case KEY_BACKSPACE:
        // Delete the character at the current position,
        // move to a new position and update the counters
        if (cursor > 0) {
          int newX, newY;
          if (x == 0) {
            // Y must be > 0 if we have text (cursor > 0)
            newX = maxX - 1;
            newY = y - 1;
          } else {
            // X > 0, Y = whatever
            newY = y;
            newX = x - 1;
          }
          if (mvwdelch(inputWin, newY, newX) != ERR) {
            cursor--;
            eom--;
            wrefresh(inputWin);
          }
          UISetInputCounter(eom, eob);
        }
      break;
      case kKeyEnter:
      case kKeyEOT: // Ctrl + D
        // Read the content of the window up to the given limit
        // and returns it to the caller function
        {
          // Points to the end of every read block
          wchar_t *cur = buffer;
          // mvwinnwstr() reads only one line at a time so we need a loop
          for (int i = 0; i < maxY; i++) {
            // eob - (cur - buffer) = remaning available unread bytes in the buffer
            int read = mvwinnwstr(inputWin, i, 0, cur, (eob - (cur - buffer)));
            if (read != ERR) {
              cur += read;
            }
          }
          // Once the input message is collected, clean the window
          wmove(inputWin, 0, 0);
          wclear(inputWin);
          wrefresh(inputWin);
          // ...and return it to the caller
          if ((cur - buffer) > 0) {
            return wcslen(buffer) + 1;
          }
        }
      break;
      case kKeyESC:
        // Cancel any operation and reset everything
        wmove(inputWin, 0, 0);
        wclear(inputWin);
        wrefresh(inputWin);
        cursor = 0;
        eom = 0;
        UISetInputCounter(eom, eob);
      break;
      case KEY_LEFT:
        // Advance the cursor if possible
        if (y > 0 && x == 0) {
          // The text cursor is at the beginning of line 2+,
          // move at the end of the previous line
          if (wmove(inputWin, y - 1, maxX - 1) != ERR) {
            wrefresh(inputWin);
            if (cursor > 0) cursor--;
          }
        } else if (x > 0) {
          // The text cursor is in the middle of a line,
          // just move to the left by one step
          if (wmove(inputWin, y, x - 1) != ERR) {
            wrefresh(inputWin);
            if (cursor > 0) cursor--;
          }
        }
        UISetInputCounter(eom, eob);
      break;
      case KEY_RIGHT:
        // Move the cursor back, if possible
        // We can move to the right only of there is already text
        if (eom > ((y * maxX) + x)) {
          if (wmove(inputWin, y, x + 1) != ERR) {
            wrefresh(inputWin);
            cursor++;
            // Cursor cannot be greater than the end of message
            if (cursor > eom) cursor = eom;
          }
        }
      break;
      case KEY_UP:
        // Move the cursor to the line above, if possible
        if (y > 0) {
          if (wmove(inputWin, y - 1, x) != ERR) {
            wrefresh(inputWin);
            cursor -= maxX;
          }
        }
      break;
      case KEY_DOWN:
        // Move the cursor to the line below,
        // but only if there is enough text in the line below
        if (y < (maxY - 1) && eom >= (maxX + x)) {
          if (wmove(inputWin, y + 1, x) != ERR) {
            wrefresh(inputWin);
            cursor += maxX;
          }
        }
      break;
      default:
        // If we have space, AND the input character is not a control char,
        // add the new character to the message window
        if (cursor < eob && !iswcntrl(ch)) {
          // Appending content to the end of the line
          if (cursor == eom && (wprintw(inputWin, "%lc", ch) != ERR)) {
            cursor++;
            eom++;
          }
          // Inserting content in the middle of a line
          if (cursor < eom && (winsch(inputWin, ch) != ERR)) {
            if (x < maxX && wmove(inputWin, y, x + 1) != ERR) {
              cursor++;
              eom++;
            } else if (wmove(inputWin, y + 1, 0) != ERR) {
              cursor++;
              eom++;
            }
            // Don't update the cursor if you can't move
          }
          wrefresh(inputWin);
          UISetInputCounter(eom, eob);
        }
      break;
    }
  }
  // An input error happened, cleanup and return error
  memset(buffer, 0, length * sizeof(wchar_t));
  wclear(inputWin);
  wrefresh(inputWin);
  return -1;
}

/**
 * Returns a custom color for a given user name
 */
int UIGetUserColor(char *userName) {
  int colors[] = {
    kColorPairDefault,
    kColorPairCyanOnDefault,
    kColorPairYellowOnDefault,
    kColorPairBlueOnDefault,
    kColorPairMagentaOnDefault,
    kColorPairGreenOnDefault
  };

  // Initialise a default color using a random number
  // from kColorPairDefault (colors[0])
  // to kColorPairGreenOnDefault (colors[5])
  static int nextColor = -1;
  if (nextColor < 0) nextColor = rand() % 6;

  // Check if a user already has a color
  int *color = (int *)Hash_getValue(users, userName);
  if (color == NULL) {
    // First time we see this user, use the next available color
    int userColor = colors[nextColor];

    // Update the next color
    if (++nextColor > 5) nextColor = 0;

    // Add the user's color to the hash
    if (!Hash_set(users, userName, &userColor, sizeof(int))) {
      userColor = 0; // Just in case, we return the default
    }
    return userColor;
  }
  // Return a copy of the value pointed by color
  return *color;
}

/**
 * Writes a message to the chat log window
 */
void UILogMessage(char *buffer, size_t length) {
  // Compute local time
  time_t now = time(NULL);
  char timeBuffer[15] = {0};
  int result = strftime(timeBuffer, 15, "%H:%M:%S", localtime(&now));
  if (result <= 0) memset(timeBuffer, 0, 15);

  // Display the message
  int y, x;
  getyx(inputWin, y, x);
  char *messageContent = NULL;
  switch (Message_getType(buffer)) {
    case kMessageTypeErr:
      messageContent = Message_getContent(buffer, kMessageTypeErr, length);
      wattron(chatWin, COLOR_PAIR(kColorPairWhiteOnRed));
      wprintw(chatWin, "[%s] [ERROR] %s\n", timeBuffer, messageContent);
      wattroff(chatWin, COLOR_PAIR(kColorPairWhiteOnRed));
    break;
    case kMessageTypeOk:
      messageContent = Message_getContent(buffer, kMessageTypeOk, length);
      if (strlen(messageContent) > 0) {
        wattron(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
        wprintw(chatWin, "[%s] [SERVER] %s\n", timeBuffer, messageContent);
        wattroff(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
      }
    break;
    case kMessageTypeLog:
      messageContent = Message_getContent(buffer, kMessageTypeLog, length);
      wattron(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
      wprintw(chatWin, "[%s] [SERVER] %s\n", timeBuffer, messageContent);
      // Intercept user disconnection messace to get user name from the message i
      // and remove it from the users hash
      if (strstr(messageContent, "left the chat") != NULL) {
        char userName[kMaxNicknameSize + 1] = {0};
        if (Message_getUser(buffer, userName, kMaxNicknameSize)) {
          if (!Hash_delete(users, userName)) {
            // Something should happen here, maybe log this?
            wprintw(
              chatWin,
              "[%s] [SERVER] Unable to remove user '%s' from internal hash\n",
              timeBuffer,
              userName
            );
          }
        }
      }
      wattroff(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
    break;
    case kMessageTypeMsg:
      messageContent = Message_getContent(buffer, kMessageTypeMsg, length);
      // Get user from message
      int userColor = kColorPairDefault;
      {
        char userName[kMaxNicknameSize + 1] = {0};
        if (Message_getUser(buffer, userName, kMaxNicknameSize)) {
          // Get/set color associated to user
          userColor = UIGetUserColor(userName);
        }
      }
      // Activate color mode
      wattron(chatWin, COLOR_PAIR(userColor));
      wprintw(chatWin, "[%s] %s\n", timeBuffer, messageContent);
      // Deactivate color mode
      wattroff(chatWin, COLOR_PAIR(userColor));
    break;
    case kMessageTypeQuit:
      // TODO: close the chat
      break;
    break;
    default:
      // Print up to byte_received from the server
      wprintw(chatWin, "Received (%d bytes): %.*s\n", length, length, buffer);
    break;
  }
  wrefresh(chatWin);
  Message_free(&messageContent);
  wmove(inputWin, y, x);
  wrefresh(inputWin);
}

/**
 * Updates the content of the status bar
 */
void UISetStatusMessage(char *buffer, size_t length) {
  // Considers 80% of the status bar available
  size_t size = (length < (size_t)(COLS * 0.8)) ? length : (COLS * 0.8) - 1;
  if (mvwprintw(statusBarWin, 0, 1, "%.*s", size, buffer) != ERR) {
    wrefresh(statusBarWin);
    memcpy(currentStatusBarMessage, buffer, ((length < 512) ? length : 512));
  }
}

/**
 * Updates the message length counter in the status bar
 * using the format <current char>/<max chars>
 */
void UISetInputCounter(int current, int max) {
  int y, x;
  getyx(inputWin, y, x);
  char text[20] = {0};
  size_t length = sprintf(text, "%4d/%d", current, max);
  mvwprintw(statusBarWin, 0, (COLS - length - 1), "%s", text);
  wrefresh(statusBarWin);
  wmove(inputWin, y, x);
  wrefresh(inputWin);
}

/**
 * Displays an error message when the current terminal
 * is too small to contain the chat
 */
void UIDrawTermTooSmall() {
  char *message = "Sorry, your terminal is too small!";
  size_t messageSize = strlen(message);
  mvwprintw(mainWin, (LINES/2), (COLS/2 - messageSize/2), "%s", message);
  mvwprintw(mainWin, (LINES/2 + 1), (COLS/2 - 3), "%dx%d", LINES, COLS);
  wrefresh(mainWin);
}

/**
 * Intercepts terminal resize events
 */
void UIResizeHandler(int signal) {
  fprintf(stderr, "Received %d\n", signal);

  // End current windows
  endwin();
  refresh();
  clear();

  if (UITermIsBigEnough()) {
    UIDrawChatWin();
    UIDrawInputWin();
    UIDrawStatusBar();
    if (strlen(currentStatusBarMessage) > 0) {
      UISetStatusMessage(currentStatusBarMessage, strlen(currentStatusBarMessage));
    }
    // Refresh and move cursor to the input window
    UILoopInit();
    return;
  }
  UIDrawTermTooSmall();
}
