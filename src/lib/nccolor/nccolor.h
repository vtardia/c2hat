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

#ifndef NC_COLOR_H
#define NC_COLOR_H

  typedef struct {
    short red;
    short green;
    short blue;
  } NCColor;

  typedef struct {
    short red;
    short green;
    short blue;
  } RGBColor;

  // Converts NC color values to RGB
  short nc2rgb(short val);

  // Converts RGB color values to NC
  short rgb2nc(short val);

  // Calculates luminance for an RGB color value
  double luminance(short r, short g, short b);

  // Calculates contrast ratio between 2 luminances
  double contrast(double lum1, double lum2);


  // Generates a random NCurses color
  NCColor nc_random_color(int seed);

  // Generates a random RGB color
  RGBColor rgb_random_color(int seed);

  // Converts an RGB color into a NCurses color
  NCColor rgb2nc_color(RGBColor rgb);

  // Converts an NCurses color into an RGB color
  RGBColor nc2rgb_color(NCColor color);

  // Finds the intensity of the RGB components in a color
  // and returns an NC-style color
  NCColor nc_color_content(short color);
#endif
