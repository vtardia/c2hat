#ifndef UI_COLOR_H
#define UI_COLOR_H

  #include <ncurses.h>

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

  /// Initialise the color engine
  void UIColor_init();

  /// Returns the maximum number of available color pairs
  int UIColor_getCount();

#endif
