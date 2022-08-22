#include "uistatus.h"
#include "uicolor.h"
#include <string.h>
#include "logger/logger.h"

typedef struct {
  WINDOW *handle;
  char message[512];
  char right[20];
  int cols;
} UIStatusBar;

UIStatusBar statusBar = {};

// Displays the status message leaving space for the input counter
#define UIStatus_update() (mvwprintw(statusBar.handle, 0, 1, "%.*s", (int)(statusBar.cols * 0.8), statusBar.message))

/// Creates or resize the status bar
void UIStatusBar_render(const UIScreen *screen) {
  if (statusBar.handle == NULL) {
    // h, w, posY, posX
    statusBar.handle = subwin(screen->handle, 1, screen->cols, screen->lines -1, 0);
    leaveok(statusBar.handle, TRUE);
  }
  statusBar.cols = screen->cols; // resize code should be places before this

  // Set window default background
  wbkgd(statusBar.handle, COLOR_PAIR(kColorPairWhiteOnBlue));

  if (strlen(statusBar.message) > 0) UIStatus_update();

  wrefresh(statusBar.handle);
}

/// Destructor
void UIStatusBar_destroy() {
  UIWindow_destroy(statusBar.handle);
  memset(&statusBar, 0, sizeof(UIStatusBar));
}

/// Sets the status bar text at the given position
bool UIStatus_set(UIStatusPosition position, char *format, ...) {
  bool updated = false;
  va_list args;
  va_start(args, format);

  if (position == kUIStatusPositionRight) {
    vsnprintf(
      statusBar.right,
      sizeof(statusBar.right),
      format,
      args
    );
    updated = (mvwprintw(
      statusBar.handle,
      0, statusBar.cols - strlen(statusBar.right) -1,
      "%s", statusBar.right
    ) != ERR);
  } else {
    // Default, update the left side
    vsnprintf(
      statusBar.message,
      sizeof(statusBar.message),
      format,
      args
    );
    updated = (UIStatus_update() != ERR);
  }

  va_end(args);
  wrefresh(statusBar.handle);
  return updated;
}
