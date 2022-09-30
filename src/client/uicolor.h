/**
 * Copyright (C) 2020-2022 Vito Tardia <https://vito.tardia.me>
 *
 * This file is part of C2Hat
 *
 * C2Hat is a simple client/server TCP chat written in C
 *
 * C2Hat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * C2Hat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with C2Hat. If not, see <https://www.gnu.org/licenses/>.
 */

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
