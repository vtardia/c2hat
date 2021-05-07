#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "ui.h"
#include "message/message.h"

enum {
  kKeyEnter = 10,
  kKeyBackspace = 8,
  kKeyDel = 127,
  kKeyEOT = 4 // Ctrl+D = end of input
};

static WINDOW *mainWin, *chatWin, *inputWin, *chatWinBox, *inputWinBox;

void UIColors();
void UIDrawChatWin();
void UIDrawInputWin();
void UIDrawTermTooSmall();

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
  chatWinBox = subwin(mainWin, (LINES * 0.8), COLS, 0, 0);
  box(chatWinBox, 0, 0);
  wrefresh(chatWinBox);
  // Chat log box, within the chat window
  chatWin = subwin(chatWinBox, (LINES * 0.8) -2, COLS -2, 1, 1);
  scrollok(chatWin, TRUE);
}

void UIDrawInputWin() {
  // Input box container: 100% wide, 20% tall, starts at the bottom of the chat box
  inputWinBox = subwin(mainWin, (LINES * 0.2), COLS, (LINES * 0.8), 0);
  box(inputWinBox, 0, 0);
  wrefresh(inputWinBox);
  // Input box, within the container
  inputWin = subwin(inputWinBox, (LINES * 0.2) - 2, COLS - 2, (LINES * 0.8) + 1, 1);
}

void UILoopInit() {
  wrefresh(chatWin);
  wcursyncup(inputWin);
  wrefresh(inputWin);
}

int UIGetUserInput(char *buffer, size_t length) {
  int y, x, maxY, maxX;
  int ch;
  char *cursor = buffer;
  char *end = buffer + length - 1;
  *end = 0;
  wmove(inputWin, 0, 0);
  wrefresh(inputWin);
  while ((ch = getch()) != KEY_F(1)) {
    if (ch == KEY_RESIZE) continue;
    switch(ch) {
      case kKeyBackspace:
      case kKeyDel:
         if (end - cursor > 0) {
            wprintw(inputWin, "\b \b\0");
            *(--cursor) = 0;
            wrefresh(inputWin);
         } else {
            wprintw(inputWin, "\b \0");
         }
      break;
      case kKeyEOT: // Ctrl + D
        // Add a null terminator
        if (cursor < end) *cursor = 0;
        // Clear window and pass the input to the caller
        wclear(inputWin);
        wrefresh(inputWin);
        return cursor - buffer + 1;
      break;
      case KEY_UP:
      case KEY_DOWN:
      case KEY_LEFT:
      case KEY_RIGHT:
        // Ignoring arrow keys and scrolling for now, too complicated
        getyx(inputWin, y, x);
        getmaxyx(inputWin, maxY, maxX);
        // wscrl(inputWin, 1);
        // wrefresh(inputWin);
        break;
      case ERR:
        // Display some message in the status bar
      break;
      default:
        // If we have space, add a character to the message
        if (cursor < end && (ch == kKeyEnter || (ch > 31 && ch <= 255))) {
          *cursor = ch;
          cursor++;
          wprintw(inputWin, (char *)&ch);
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

    // Refresh and move cursor to input window
    wrefresh(chatWin);
    wcursyncup(inputWin);
    wrefresh(inputWin);
  }
}