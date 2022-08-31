#include "uistatus.h"
#include "uicolor.h"
#include <string.h>

typedef struct {
  WINDOW *handle;
  int lines;
  int cols;
  char mode[4];      //< Chatlog mode: live/browse
  char size[12];     //< Terminal size [lines,columns]
  char message[512]; //< Message
  char counter[10];  //< Input counter [count/total]
} UIStatusBar;

UIStatusBar this = {};

// Displays the status message leaving space for the input counter
#define UIStatus_updateMode() (mvwprintw(this.handle, 0, 1, "[%s]", this.mode))
#define UIStatus_updateSize() (mvwprintw(this.handle, 0, 5, "[%s]", this.size))
#define UIStatus_updateMessage() (mvwprintw(this.handle, 0, 14, "%.*s", (int)(this.cols - strlen(this.counter)), this.message))
#define UIStatus_updateCounter() (mvwprintw(this.handle, 0, this.cols - strlen(this.counter) -1, "%s", this.counter))

/// Creates or resize the status bar
void UIStatusBar_render(const UIScreen *screen) {
  if (this.handle != NULL) UIWindow_destroy(this.handle);
  this.handle = derwin(screen->handle, 1, screen->cols, screen->lines -1, 0);
  getmaxyx(this.handle, this.lines, this.cols);

  // Set window default background
  wbkgd(this.handle, COLOR_PAIR(kColorPairWhiteOnBlue));

  if (strlen(this.mode) > 0) UIStatus_updateMode();
  if (strlen(this.size) > 0) UIStatus_updateSize();
  if (strlen(this.message) > 0) UIStatus_updateMessage();
  if (strlen(this.counter) > 0) UIStatus_updateCounter();

  wnoutrefresh(this.handle);
}

/// Destructor
void UIStatusBar_destroy() {
  UIWindow_destroy(this.handle);
  memset(&this, 0, sizeof(UIStatusBar));
}

/// Sets the status bar text for the given area
bool UIStatus_set(UIStatusArea area, char *format, ...) {
  bool updated = false;
  va_list args;
  va_start(args, format);

  #define SET(var) vsnprintf(var, sizeof(var), format, args)

  if (area == kUIStatusMode) {
    SET(this.mode);
    updated = (UIStatus_updateMode() != ERR);
  } else if (area == kUIStatusTerminalSize) {
    SET(this.size);
    updated = (UIStatus_updateSize() != ERR);
  } else if (area == kUIStatusinputCounter) {
    SET(this.counter);
    updated = (UIStatus_updateCounter() != ERR);
  } else if (area == kUIStatusMessage) {
    // Default, update the left side
    SET(this.message);
    updated = (UIStatus_updateMessage() != ERR);
  }

  va_end(args);
  wrefresh(this.handle);
  return updated;
}

void UIStatus_get(char *buffer, size_t length) {
  memcpy(buffer, this.message, length);
}
