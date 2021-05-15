#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "ui.h"
#include "message/message.h"

enum {
  kKeyEnter = 10,
  kKeyBackspace = 8,
  kKeyESC = 27,
  kKeyDel = 127,
  kKeyEOT = 4 // Ctrl+D = end of input
};

static WINDOW *mainWin, *chatWin, *inputWin, *chatWinBox, *inputWinBox, *statusBarWin;

void UIColors();
void UIDrawChatWin();
void UIDrawInputWin();
void UIDrawStatusBar();
void UIDrawTermTooSmall();
void UISetInputCounter(int, int);

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

  // Initialise colors
  UIColors();

  if (LINES < 24 || COLS < 76) {
    UIDrawTermTooSmall();
  } else {
    UIDrawChatWin();
    UIDrawInputWin();
    UIDrawStatusBar();
  }
}

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
}

void UIColors() {
  // Init ncurses color engine
  start_color();
  use_default_colors();
}

void UIDrawChatWin() {
  // Chat window container: 100% wide, 80% tall, starts at top left
  chatWinBox = subwin(mainWin, (LINES - 6), COLS, 0, 0);
  box(chatWinBox, 0, 0);

  // Draw title
  const char *title = " C2Hat ";
  size_t titleLength = strlen(title);
  mvwaddch(chatWinBox, 0, (COLS/2) - titleLength/2 - 1 , ACS_RTEE);
  mvwaddstr(chatWinBox, 0, (COLS/2) - titleLength/2, title);
  mvwaddch(chatWinBox, 0, (COLS/2) + titleLength/2 + 1, ACS_LTEE);
  wrefresh(chatWinBox);

  // Chat log box, within the chat window
  chatWin = subwin(chatWinBox, (LINES - 8), COLS -2, 1, 1);
  scrollok(chatWin, TRUE);
  leaveok(chatWin, TRUE);
}

void UIDrawInputWin() {
  // Input box container: 100% wide, 20% tall, starts at the bottom of the chat box
  inputWinBox = subwin(mainWin, (5), COLS, (LINES -6), 0);
  box(inputWinBox, 0, 0);
  wrefresh(inputWinBox);
  // Input box, within the container
  inputWin = subwin(inputWinBox, (3), COLS - 2, (LINES - 6) + 1, 1);
}

void UIDrawStatusBar() {
  // h, w, posY, posX
  statusBarWin = subwin(mainWin, 1, COLS, LINES -1, 0);
  leaveok(statusBarWin, TRUE);
  wrefresh(statusBarWin);
}

void UILoopInit() {
  wrefresh(chatWin);
  wcursyncup(inputWin);
  wrefresh(inputWin);
}

int UIGetUserInput(char *buffer, size_t length) {
  int y, x, maxY, maxX;
  int ch;

  // Variable pointer that follow the cursor on the window
  int cursor = 0;

  // Variable pointer that point to the end of the typed message
  int eom = 0;

  // Keeps track of character limit
  int eob = length -1;

  // Fixed pointer to the very end of the buffer
  char *end = buffer + eob;

  // Add a safe NULL terminator at the end of buffer
  *end = 0;

  // Initialise the input window and counters
  wmove(inputWin, 0, 0);
  wclear(inputWin);
  wrefresh(inputWin);
  UISetInputCounter(eom, eob);

  // Wait for input
  while ((ch = getch()) != KEY_F(1)) {
    if (ch == KEY_RESIZE) continue;
    getyx(inputWin, y, x);
    getmaxyx(inputWin, maxY, maxX);
    switch(ch) {
      case kKeyBackspace:
      case kKeyDel:
      case KEY_BACKSPACE:
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
      // case kKeyEOT: // Ctrl + D
        // Read the content of the window up to a max
        // mvwinnstr() reads only one line at a time so we need a loop
        {
          // Points to the end of every read block
          char *cur = buffer;
          for (int i = 0; i < maxY; i++) {
            // eob - (cur - buffer) = remaning available unread bytes in the buffer
            int read = mvwinnstr(inputWin, i, 0, cur, (eob - (cur - buffer)));
            if (read != ERR) {
              cur += read;
            }
          }
          if ((cur - buffer) > 0) {
            wmove(inputWin, 0, 0);
            return strlen(buffer) + 1;
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
        if (y > 0) {
          if (wmove(inputWin, y - 1, x) != ERR) {
            wrefresh(inputWin);
            cursor -= maxX;
          }
        }
      break;
      case KEY_DOWN:
        // We can move only if there is enogh text in the line below
        if (y < (maxY - 1) && eom >= (maxX + x)) {
          if (wmove(inputWin, y + 1, x) != ERR) {
            wrefresh(inputWin);
            cursor += maxX;
          }
        }
      break;
      case ERR:
        // Display some message in the status bar
      break;
      default:
        // If we have space, add a character to the message
        if (cursor < eob && (ch > 31 && ch <= 255)) {
          // Appending content to the end of the line
          if (cursor == eom && (wprintw(inputWin, (char *)&ch) != ERR)) {
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
        } else {
          wmove(inputWin, 0, maxX/2);
          wprintw(inputWin, "%d", ch);
          wrefresh(inputWin);
        }
      break;
    }
  }
  memset(buffer, 0, length);
  wclear(inputWin);
  wrefresh(inputWin);
  return -1;
}

void UILogMessage(char *buffer, size_t length) {
  int y, x;
  getyx(inputWin, y, x);
  char *messageContent = NULL;
  switch (Message_getType(buffer)) {
    case kMessageTypeErr:
      messageContent = Message_getContent(buffer, kMessageTypeErr, length);
      wprintw(chatWin, "[Error]: %s\n", messageContent);
    break;
    case kMessageTypeOk:
      messageContent = Message_getContent(buffer, kMessageTypeOk, length);
      if (strlen(messageContent) > 0) {
        wprintw(chatWin, "[Server]: %s\n", messageContent);
      }
    break;
    case kMessageTypeLog:
      messageContent = Message_getContent(buffer, kMessageTypeLog, length);
      wprintw(chatWin, "[Server]: %s\n", messageContent);
    break;
    case kMessageTypeMsg:
      messageContent = Message_getContent(buffer, kMessageTypeMsg, length);
      wprintw(chatWin, "%s\n", messageContent);
    break;
    case kMessageTypeQuit:
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

void UISetStatusMessage(char *buffer, size_t length) {
  // Considers 80% of the status bar available
  size_t size = (length < (size_t)(COLS * 0.8)) ? length : (COLS * 0.8) - 1;
  mvwprintw(statusBarWin, 0, 1, "%.*s", size, buffer);
  wrefresh(statusBarWin);
}

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

void UIDrawTermTooSmall() {
  char *message = "Sorry, your terminal is too small!";
  size_t messageSize = strlen(message);
  mvwprintw(mainWin, (LINES/2), (COLS/2 - messageSize/2), "%s", message);
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

  if (LINES < 24 || COLS < 76) {
    UIDrawTermTooSmall();
  } else {
    UIDrawChatWin();
    UIDrawInputWin();
    UIDrawStatusBar();

    // Refresh and move cursor to input window
    wrefresh(chatWin);
    wcursyncup(inputWin);
    wrefresh(inputWin);
  }
}
