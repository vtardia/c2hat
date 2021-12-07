/*
 * Copyright (C) 2021 Vito Tardia
 */

// TODO: extract the message char counter out of the UserInputLoop

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <wctype.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>

#include "../c2hat.h"
#include "ui.h"
#include "message/message.h"
#include "hash/hash.h"
#include "list/list.h"
#include "queue/queue.h"
#include "uilog.h"

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

enum config {
  /// Max message length, in characters, including the NULL terminator
  kMaxMessageLength = 281,
  /// Min lines to be available in the terminal (eg. 24x80 term has 22 lines available)
  kMinTerminalLines = 22,
  /// Min columns to be available in the terminal
  kMinTerminalCols = 80,
  /// Min columns to be considered for a wide terminal
  kWideTerminalCols = 94,
  /// Max cached lines for the chat log window
  kMaxCachedLines = 100
};

enum chatWinStatusType {
  /// The chat window is currently receiving messages in real time
  kChatWinStatusLive = 1,
  /// The user is currently navigating the chat window with page keys
  kChatWinStatusBrowse = 2
};

static WINDOW *mainWin, *chatWin, *inputWin, *chatWinBox, *inputWinBox, *statusBarWin;
static char currentStatusBarMessage[512] = {0};

/// Associative array where the keys are the user nicknames
static Hash *users = NULL;

/// Dynamic list that caches the chatlog content
static List *messages = NULL;

/// Dynamic queue for message events
static Queue *events = NULL;

/// Flag used by the user input loop to jump out of get_wch()
static bool uiTerminate = false;

/// Keeps track of chat window status
static enum chatWinStatusType chatWinStatus = kChatWinStatusLive;

/// Available lines in the main window
static int screenLines = 0;

/// Available columns in the main window
static int screenCols = 0;

/// Height of the chat window container
static int chatWinBoxHeight = 0;

/// Total lines in the chat window
static int chatWinLines = 0;

/// Total columns in the chat window
static int chatWinCols = 0;

/// Available lines in the chat window (usually lines -1)
static int chatWinPageSize = 0;

/// Keeps track of the current start line when in browse mode
static int chatLogCurrentLine = 0;

/// Height of the input window container
static int inputWinBoxHeight = 0;

/// Y coordinate start for the input window container
static int inputWinBoxStart = 0;

// Mutex for message buffer
pthread_mutex_t messagesLock = PTHREAD_MUTEX_INITIALIZER;

// Mutex for events queue
pthread_mutex_t eventsLock = PTHREAD_MUTEX_INITIALIZER;

// Mutex for the UI
pthread_mutex_t uiLock = PTHREAD_MUTEX_INITIALIZER;

void UIColors();
void UIDrawChatWin();
void UIDrawInputWin();
void UIDrawStatusBar();
void UIDrawTermTooSmall();
void UISetInputCounter(int, int);
void UILogMessageDisplay(ChatLogEntry *entry, bool refresh);
void UIDrawAll();

/**
 * Switches the chat window mode to Live
 */
void UISetChatModeLive() {
  if (chatWinStatus != kChatWinStatusLive) {
    pthread_mutex_lock(&messagesLock);
    chatWinStatus = kChatWinStatusLive;
    if (messages && messages->length > 0) {
      chatLogCurrentLine = messages->length -1;
    }
    // Update status bar
    mvwprintw(statusBarWin, 0, 2, "%s", "C");
    wrefresh(statusBarWin);
    UILoopInit();
    pthread_mutex_unlock(&messagesLock);
  }
}

/**
 * Switches the chat window mode to Browse
 */
void UISetChatModeBrowse() {
  pthread_mutex_lock(&messagesLock);
  if (chatWinStatus != kChatWinStatusBrowse && messages && messages->length > chatWinLines) {
    chatWinStatus = kChatWinStatusBrowse;
    // Position the line pointer at the start ot the page
    chatLogCurrentLine = messages->length - chatWinLines;
    // Update status bar
    mvwprintw(statusBarWin, 0, 2, "%s", "B");
    wrefresh(statusBarWin);
    UILoopInit();
  }
  pthread_mutex_unlock(&messagesLock);
}

/**
 * Get the number of available input lines
 * based on the terminal width
 */
int UIGetInputLines() {
  return (screenCols < kWideTerminalCols) ? 4  : 3;
}

/**
 * Checks if we have enough space within the terminal
 * We need at least a 24x80 terminal that can host 280
 * characters in the input window
 */
bool UITermIsBigEnough() {
  int lines = UIGetInputLines();
  return (screenLines > kMinTerminalLines && screenCols > kMinTerminalCols && ((screenCols - 1) * lines) >= kMaxMessageLength);
}

/**
 * Clears the console
 */
void UIClearScreen() {
  printf("\e[1;1H\e[2J");
}

/**
 * Initialises NCurses configuration
 */
void UIInit() {
  if (mainWin != NULL) return; // Already initialised

  UIClearScreen();

  if ((mainWin = initscr()) == NULL) {
    fprintf(stderr, "❌ Error: Unable to initialise the user interface\n%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  getmaxyx(mainWin, screenLines, screenCols);

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

  // Initialise Users hash
  users = Hash_new();
  if (users == NULL) {
    fprintf(stderr, "❌ Error: Unable to initialise the users list\n%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Initialise chat buffer
  messages = List_new();
  if (messages == NULL) {
    fprintf(stderr, "❌ Error: Unable to initialise the chat log buffer\n%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Initialise events queue
  events = Queue_new();
  if (events == NULL) {
    fprintf(stderr, "❌ Error: Unable to initialise the events queue\n%s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Initialises the random generator
  srand(time(NULL));

  // Draw the interface (after all init)
  UIDrawAll();
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
  UIWindow_destroy(statusBarWin);
  UIWindow_destroy(inputWin);
  UIWindow_destroy(inputWinBox);
  UIWindow_destroy(chatWin);
  UIWindow_destroy(chatWinBox);
  UIWindow_destroy(mainWin);
  // Close ncurses
  endwin();
  // Free user list and message buffer
  Hash_free(&users);
  List_free(&messages);
  Queue_free(&events);
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
 * Fill the chat/log window with content from the buffer
 */
void UIDrawChatWinContent() {
  pthread_mutex_lock(&messagesLock);
  if (chatWin && messages && messages->length > 0) {
    pthread_mutex_lock(&uiLock);
    // Writing on the last line will make the window scroll
    int start = 0;
    wclear(chatWin);
    wmove(chatWin, 0, 0);

    if (chatWinStatus == kChatWinStatusLive) {
      start = messages->length - chatWinPageSize;
      if (start < 0) start = 0;
      for (int line = start; line < messages->length; line++) {
        ChatLogEntry *entry = (ChatLogEntry *) List_item(messages, line);
        if (entry != NULL) {
          UILogMessageDisplay(entry, false);
        }
      }
    } else if (chatWinStatus == kChatWinStatusBrowse) {
      // A page up/down key handler will manage the current line pointer
      start = chatLogCurrentLine;
      int end = start + chatWinPageSize;
      if (end >= messages->length) end = messages->length;
      int line = start;
      while (line < end) {
        ChatLogEntry *entry = (ChatLogEntry *) List_item(messages, line);
        if (entry != NULL) {
          UILogMessageDisplay(entry, false);
        }
        line++;
      }
    }
    wrefresh(chatWin);
    pthread_mutex_unlock(&uiLock);
  }
  pthread_mutex_unlock(&messagesLock);
}

/**
 * Draws the chat/log window
 */
void UIDrawChatWin() {
  pthread_mutex_lock(&uiLock);

  if (chatWin != NULL) UIWindow_destroy(chatWin);
  if (chatWinBox != NULL) UIWindow_destroy(chatWinBox);

  // Create the chat window container: 80% tall, 100% wide, starts at top left
  chatWinBox = subwin(mainWin, chatWinBoxHeight, screenCols, 0, 0);

  // Add border, just top and bottom to avoid breaking the layout
  // when the user inserts emojis
  // win, left side, right side, top side, bottom side,
  // corners: top left, top right, bottom left, bottom right
  wborder(chatWinBox, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');

  // Draw title
  const char *title = " C2Hat ";
  size_t titleLength = strlen(title);
  mvwaddch(chatWinBox, 0, (screenCols/2) - titleLength/2 - 1 , ACS_RTEE);
  mvwaddstr(chatWinBox, 0, (screenCols/2) - titleLength/2, title);
  mvwaddch(chatWinBox, 0, (screenCols/2) + titleLength/2 + 1, ACS_LTEE);
  wrefresh(chatWinBox);

  // Draw the scrollable chat log box, within the chat window
  chatWin = subwin(chatWinBox, (chatWinBoxHeight - 1), (screenCols - 2), 1, 1);
  scrollok(chatWin, TRUE);
  leaveok(chatWin, TRUE);
  wrefresh(chatWin);
  getmaxyx(chatWin, chatWinLines, chatWinCols);

  // Available display lines
  // (writing on the last line will make the window scroll)
  chatWinPageSize = chatWinLines - 1;

  pthread_mutex_unlock(&uiLock);
  // Draw the content inside the window, if present
  UIDrawChatWinContent();
}

/**
 * Draws the chat input window
 */
void UIDrawInputWin() {
  pthread_mutex_lock(&uiLock);

  if (inputWin != NULL) UIWindow_destroy(inputWin);
  if (inputWinBox != NULL) UIWindow_destroy(inputWinBox);

  // Input box container: 20% tall, 100% wide, starts at the bottom of the chat box
  inputWinBox = subwin(mainWin, inputWinBoxHeight, screenCols, inputWinBoxStart, 0);
  wborder(inputWinBox, ' ', ' ', 0, ' ', ' ', ' ', ' ', ' ');
  wrefresh(inputWinBox);

  // Input box, within the container
  inputWin = subwin(inputWinBox, (inputWinBoxHeight - 2), (screenCols - 2), (inputWinBoxStart + 1), 1);
  wrefresh(inputWin);
  pthread_mutex_unlock(&uiLock);
}

/**
 * Draws the status bar as last line of the screen
 */
void UIDrawStatusBar() {
  pthread_mutex_lock(&uiLock);
  if (statusBarWin != NULL) UIWindow_destroy(statusBarWin);

  // h, w, posY, posX
  statusBarWin = subwin(mainWin, 1, screenCols, screenLines -1, 0);
  leaveok(statusBarWin, TRUE);
  // Set window default background
  wbkgd(statusBarWin, COLOR_PAIR(kColorPairWhiteOnBlue));
  wrefresh(statusBarWin);
  pthread_mutex_unlock(&uiLock);
}

/**
 * Refresh the UI and move the cursor
 * to the input window to receive input
 */
void UILoopInit() {
  pthread_mutex_lock(&uiLock);
  if (chatWin && inputWin) {
    wrefresh(chatWin);
    wcursyncup(inputWin);
    wrefresh(inputWin);
  }
  pthread_mutex_unlock(&uiLock);
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
  pthread_mutex_lock(&uiLock);
  if (inputWin) {
    wmove(inputWin, 0, 0);
    wclear(inputWin);
    wrefresh(inputWin);
    getmaxyx(inputWin, maxY, maxX);
    if (eob > (maxX * maxY)) {
      eob = maxX * maxY;
    }
  }
  pthread_mutex_unlock(&uiLock);
  UISetInputCounter(eom, eob);

  // Wait for input
  while (true) {
    if (inputWin == NULL) continue;
    // Receives a Unicode character from the user
    int res = get_wch(&ch);
    if (res == ERR) {
      // EINTR means a signal is received, for example resize (which we are ignoring here)
      if (errno != EINTR) {
        // We may want to know what the error was (TODO: maybe use a logger here)
        fprintf(stdout, "Error: %d - %s\n", errno, strerror(errno));
        break;
      }

      // We need to process the SIGUSR1 output that tells us to terminate
      if (/* errno == EINTR && */ uiTerminate) {
        sleep(2); // Allow the user to read the error message on the chat log
        break;
      }
    }

    // Exit on F1
    if (ch == KEY_F(1)) break;

    // Ignore resize key
    if (ch == KEY_RESIZE) continue;

    // Fetch the current cursor position and window size
    if (inputWin) {
      getyx(inputWin, y, x);
      getmaxyx(inputWin, maxY, maxX);
    }

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
          if (inputWin && mvwdelch(inputWin, newY, newX) != ERR) {
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
            if (!inputWin) break;
            int read = mvwinnwstr(inputWin, i, 0, cur, (eob - (cur - buffer)));
            if (read != ERR) {
              cur += read;
            }
          }
          // Once the input message is collected, clean the window
          if (inputWin) {
            wmove(inputWin, 0, 0);
            wclear(inputWin);
            wrefresh(inputWin);
          }
          // ...and return it to the caller
          if ((cur - buffer) > 0) {
            return wcslen(buffer) + 1;
          }
        }
      break;
      case kKeyESC:
        if (chatWinStatus == kChatWinStatusBrowse) {
          // If in browse mode, exit and go live
          UISetChatModeLive();
          UIDrawChatWinContent();
        } else {
          // If in Live mode, cancel any input operation and reset everything
          if (inputWin) {
            wmove(inputWin, 0, 0);
            wclear(inputWin);
            wrefresh(inputWin);
          }
          cursor = 0;
          eom = 0;
          UISetInputCounter(eom, eob);
        }
      break;
      case KEY_LEFT:
        // Advance the cursor if possible
        if (y > 0 && x == 0) {
          // The text cursor is at the beginning of line 2+,
          // move at the end of the previous line
          if (inputWin && wmove(inputWin, y - 1, maxX - 1) != ERR) {
            wrefresh(inputWin);
            if (cursor > 0) cursor--;
          }
        } else if (x > 0) {
          // The text cursor is in the middle of a line,
          // just move to the left by one step
          if (inputWin && wmove(inputWin, y, x - 1) != ERR) {
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
          if (inputWin && wmove(inputWin, y, x + 1) != ERR) {
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
          if (inputWin && wmove(inputWin, y - 1, x) != ERR) {
            wrefresh(inputWin);
            cursor -= maxX;
          }
        }
      break;
      case KEY_DOWN:
        // Move the cursor to the line below,
        // but only if there is enough text in the line below
        if (y < (maxY - 1) && eom >= (maxX + x)) {
          if (inputWin && wmove(inputWin, y + 1, x) != ERR) {
            wrefresh(inputWin);
            cursor += maxX;
          }
        }
      break;
      case KEY_PPAGE:
        UISetChatModeBrowse();
        if (chatWinStatus == kChatWinStatusBrowse) {
          // Display the previous screen
          chatLogCurrentLine -= chatWinPageSize;
          if (chatLogCurrentLine < 0) chatLogCurrentLine = 0;
          UIDrawChatWinContent();
          if (inputWin) wrefresh(inputWin);
        }
      break;
      case KEY_NPAGE:
        if (chatWinStatus == kChatWinStatusBrowse) {
          // Display the next screen
          if (chatLogCurrentLine < (messages->length - chatWinPageSize)) {
            chatLogCurrentLine += chatWinPageSize;
            if (chatLogCurrentLine > messages->length) chatLogCurrentLine -= messages->length - 1;
            UIDrawChatWinContent();
            if (inputWin) wrefresh(inputWin);
          }
        }
      break;
      default:
        // If we have space, AND the input character is not a control char,
        // add the new character to the message window
        if (cursor < eob && !iswcntrl(ch)) {
          // Appending content to the end of the line
          if (inputWin && cursor == eom && (wprintw(inputWin, "%lc", ch) != ERR)) {
            cursor++;
            eom++;
          }
          // Inserting content in the middle of a line
          if (inputWin && cursor < eom && (winsch(inputWin, ch) != ERR)) {
            if (x < maxX && wmove(inputWin, y, x + 1) != ERR) {
              cursor++;
              eom++;
            } else if (inputWin && wmove(inputWin, y + 1, 0) != ERR) {
              cursor++;
              eom++;
            }
            // Don't update the cursor if you can't move
          }
          if (inputWin) wrefresh(inputWin);
          UISetInputCounter(eom, eob);
        }
      break;
    }
  }
  // An input error happened, cleanup and return error
  memset(buffer, 0, length * sizeof(wchar_t));
  if (inputWin) {
    wclear(inputWin);
    wrefresh(inputWin);
  }
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
 * Displays a log entry in the chat log window
 */
void UILogMessageDisplay(ChatLogEntry *entry, bool refresh) {
  if (chatWin == NULL) return;
  if (entry == NULL || entry->length == 0) return;

  // Backup input window coordinates
  int y = 0, x = 0;
  if (inputWin && refresh) getyx(inputWin, y, x);

  switch (entry->type) {
    case kMessageTypeErr:
      wattron(chatWin, COLOR_PAIR(kColorPairWhiteOnRed));
      wprintw(chatWin, "[%s] [ERROR] %s\n", entry->timestamp, entry->content);
      wattroff(chatWin, COLOR_PAIR(kColorPairWhiteOnRed));
    break;
    case kMessageTypeOk:
      wattron(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
      wprintw(chatWin, "[%s] [SERVER] %s\n", entry->timestamp, entry->content);
      wattroff(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
    break;
    case kMessageTypeLog:
      wattron(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
      wprintw(chatWin, "[%s] [SERVER] %s\n", entry->timestamp, entry->content);
      wattroff(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
    break;
    case kMessageTypeMsg: ;
      // The semicolon above prevents the
      // 'a label can only be part of a statement and a declaration is not a statement' error
      // Get/set color associated to user
      int userColor = (strlen(entry->username)) ? UIGetUserColor(entry->username) : kColorPairDefault;
      // Activate color mode
      wattron(chatWin, COLOR_PAIR(userColor));
      wprintw(chatWin, "[%s] %s\n", entry->timestamp, entry->content);
      // Deactivate color mode
      wattroff(chatWin, COLOR_PAIR(userColor));
    break;
    default:
      // Print up to byte_received from the server
      wprintw(chatWin, "Received (%zu bytes): %.*s\n", entry->length, (int) entry->length, entry->content);
    break;
  }
  if (refresh) wrefresh(chatWin);

  // Restore input window coordinates
  if (inputWin && refresh) {
    wmove(inputWin, y, x);
    wrefresh(inputWin);
  }
}

/**
 * Writes a message to the chat log window
 */
void UILogMessage(char *buffer, size_t length) {
  // Process the server data into a temporary entry
  ChatLogEntry *entry = ChatLogEntry_create(buffer, length);
  if (entry != NULL) {
    pthread_mutex_lock(&uiLock);
    // Deal with server-issued /quit command
    if (entry->type == kMessageTypeQuit) {
      if (chatWin) {
        wattron(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
        wprintw(chatWin, "[%s] [SERVER] You've been disconnected - %s\n", entry->timestamp, entry->content);
        wattroff(chatWin, COLOR_PAIR(kColorPairRedOnDefault));
      }
      ChatLogEntry_free(&entry);
      // Tells the user input loop to stop
      pthread_kill(pthread_self(), SIGUSR1);
      return;
    }

    pthread_mutex_lock(&messagesLock);
    // Append the entry to the buffer list
    List_append(messages, entry, sizeof(ChatLogEntry));
    // Delete the oldest node if we reach the max allowed buffer
    if (messages->length > kMaxCachedLines) {
      List_delete(messages, 0);
    }
    pthread_mutex_unlock(&messagesLock);

    // Do actions based on entry content (e.g. deleting a user from the Hash list)

    // Intercept user disconnection message to get user name from the message i
    // and remove it from the users hash
    if (entry->type == kMessageTypeLog && strlen(entry->username)
      && strstr(entry->content, "left the chat") != NULL) {
      if (!Hash_delete(users, entry->username)) {
        // This is temporary and should be logged on file, the user shouldn't see it
        wprintw(
          chatWin,
          "[%s] [SERVER] Unable to remove user '%s' from internal hash\n",
          entry->timestamp,
          entry->username
        );
      }
    }

    // Display the message on the log window if in 'follow' mode
    if (chatWinStatus == kChatWinStatusLive) UILogMessageDisplay(entry, true);

    pthread_mutex_unlock(&uiLock);

    // Destroy the temporary entry
    ChatLogEntry_free(&entry);
  }
}

/**
 * Updates the content of the status bar
 */
void UISetStatusMessage(char *buffer, size_t length) {
  if (statusBarWin == NULL) return;

  // Calculate the terminal size to be displayed at the bottom left
  char termSize[20] = {0};
  snprintf(termSize, 19, "[%d,%d]", screenCols, screenLines);
  size_t termSizeLength = strlen(termSize);

  // Determine the current chat window mode
  char chatWinMode[10] = {0};
  snprintf(chatWinMode, 9, "[%s]", (chatWinStatus == kChatWinStatusBrowse ? "B" : "C"));
  size_t chatWinModeLength = strlen(chatWinMode);

  // Considers 80% of the status bar available, minus the term size message
  size_t availableSpace = (screenCols * 0.8) - termSizeLength - chatWinModeLength;
  size_t size = (length < availableSpace) ? length : (availableSpace - 1);

  // Display the chat win mode
  mvwprintw(statusBarWin, 0, 1, "%s", chatWinMode);

  pthread_mutex_lock(&uiLock);
  // Display the term size
  mvwprintw(statusBarWin, 0, chatWinModeLength + 2, "%s", termSize);

  // Display the provided message after the term size
  if (mvwprintw(statusBarWin, 0, chatWinModeLength + termSizeLength + 3, "%.*s", (int)size, buffer) != ERR) {
    wrefresh(statusBarWin);
    memcpy(currentStatusBarMessage, buffer, ((length < 512) ? length : 512));
  }
  pthread_mutex_unlock(&uiLock);
}

/**
 * Updates the message length counter in the status bar
 * using the format <current char>/<max chars>
 */
void UISetInputCounter(int current, int max) {
  if (inputWin == NULL) return;
  pthread_mutex_lock(&uiLock);

  int y, x;
  getyx(inputWin, y, x);
  char text[20] = {0};
  size_t length = sprintf(text, "%4d/%d", current, max);
  mvwprintw(statusBarWin, 0, (screenCols - length - 1), "%s", text);
  wrefresh(statusBarWin);
  wmove(inputWin, y, x);
  wrefresh(inputWin);
  pthread_mutex_unlock(&uiLock);
}

/**
 * Displays an error message when the current terminal
 * is too small to contain the chat
 */
void UIDrawTermTooSmall() {
  char *message = "Sorry, your terminal is too small!";
  size_t messageSize = strlen(message);
  mvwprintw(mainWin, (screenLines/2), (screenCols/2 - messageSize/2), "%s", message);
  mvwprintw(mainWin, (screenLines/2 + 1), (screenCols/2 - 3), "%dx%d", screenLines, screenCols);
  wrefresh(mainWin);
}

/**
 * Handles custom USR1 signals (close UI)
 */
void UITerminate(int signal) {
  (void) signal;
  uiTerminate = true;
}

/**
 * Handles custom USR2 signals (new message in queue)
 */
void UIQueueHandler(int signal) {
  (void) signal;
  QueueData *item = NULL;
  if (!Queue_empty(events)) {
    do {
      pthread_mutex_lock(&eventsLock);
      item = Queue_dequeue(events);
      pthread_mutex_unlock(&eventsLock);
      if (item == NULL) break;
      UILogMessage((char*)item->content, item->length);
      QueueData_free(&item);
    } while(!Queue_empty(events));
  }
}

/**
 * Intercepts terminal resize events
 */
void UIResizeHandler(int signal) {
  (void)signal;

  // End current windows (temporary escape)
  endwin();
  // Resume visual mode
  refresh();
  clear();

  getmaxyx(mainWin, screenLines, screenCols);

  // Redraw the user interface
  UIDrawAll();
}

/**
 * Draws the whole interface
 */
void UIDrawAll() {
  // Set the sizes of container windows based on the current terminal
  // With narrow terminals we need 4 input lines to fit the whole message
  // so the chat log will be smaller. With wider terminals we can get away
  // with 3 lines for the input and leave the rest for the chat log
  inputWinBoxHeight = (screenCols < kWideTerminalCols) ? 6  : 5;
  // The upper border requires 1 line
  inputWinBoxStart = screenLines - (inputWinBoxHeight + 1);
  chatWinBoxHeight = inputWinBoxStart;

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

/**
 * Pushes a server message in the queue
 */
void UIPushMessage(char *buffer, size_t length) {
  pthread_mutex_lock(&eventsLock);
  Queue_enqueue(events, buffer, length);
  pthread_mutex_unlock(&eventsLock);
}

