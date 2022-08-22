#include "uicolor.h"
#include "logger/logger.h"
#include "nccolor/nccolor.h"

/// Keeps track of how many color pairs we have
static int pairCount = kColorPairWhiteOnRed + 1;

void UIColor_init() {
  FatalIf(!has_colors(), "Sorry, your terminal does not support colors :/");
  FatalIf(start_color() != OK, "Unable to initialise colors\n%s\n", strerror(errno));

  // Start by using the default colors for the current terminal
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

  if (can_change_color() && COLORS > kColorPairWhiteOnRed) {
    Debug("This terminal can define custom colors");

    // Fetch background color and check luminance
    short fg = 0, bg = 0;
    pair_content(kColorPairCyanOnDefault, &fg, &bg);
    NCColor bgcolor = nc_color_content(bg);
    Debug(
      "Background color id is %d (%d, %d, %d) Luminance: %f",
      bg, bgcolor.red, bgcolor.green, bgcolor.blue,
      luminance(nc2rgb(bgcolor.red), nc2rgb(bgcolor.green), nc2rgb(bgcolor.blue))
    );

    // Generate additional colors and color pairs
    // starting after the last default pair
    for (int i = 8; i < COLORS; i++) {
      // Generate a random RGB color,
      RGBColor color = rgb_random_color(i);

      // Calculate luminance and contrast ratio
      double lum = luminance(color.red, color.green, color.blue);
      double ratio = contrast(lum, 0);

      // Create a new color and color pair only if
      // the contrast ratio is compatible with the background
      if (ratio < 0.3333) {
        // Convert from RGB to NCColor
        NCColor col = rgb2nc_color(color);

        // Create color and color pair
        init_color(i, col.red, col.green, col.blue);
        init_pair(pairCount, i, -1);
        pairCount++;
      }
    }
  }
}
