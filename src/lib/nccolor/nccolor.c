#include "nccolor.h"
#include <stdlib.h>
#include <math.h>
#include <ncurses.h>

// TODO Also read https://serennu.com/colour/rgbtohsl.php
// for RGB => HSL conversion in order to find colors with the right contrast

/**
 * Converts NCurses 1000-based RGB into standard 256 RGB
 */
short nc2rgb(short val) {
  return (short)round((double)val/999*255);
}

/**
 * Converts RGB color values to NCurses 1000-based values
 */
short rgb2nc(short val) {
  return (short)round((double)val/255*999);
}

/**
 * Calculates luminance of an RGB color, ported from
 * https://dev.to/alvaromontoro/building-your-own-color-contrast-checker-4j7o
 */
double luminance(short r, short g, short b) {
  short v[] = {r, g, b};
  double a[3] = {0};
  for (int i = 0; i < 3; i++) {
    double tmp = (double)v[i] / 255;
    a[i] = (tmp <= 0.03928) ? tmp / 12.92 : pow((tmp + 0.055) / 1.055, 2.4);
  }
  return a[0] * 0.2126 + a[1] * 0.7152 + a[2] * 0.0722;
}

/**
 * Calculates the contrast ratio betweeh 2 luminance values
 */
double contrast(double lum1, double lum2) {
  return (lum1 > lum2) ?
    ((lum2 + 0.05) / (lum1 + 0.05)) :
    ((lum1 + 0.05) / (lum2 + 0.05));
}

NCColor nc_random_color(int seed) {
  short max = 999;
  srand(seed);
  NCColor color = {
    .red = (rand() % (max + 1)),
    .green = (rand() % (max + 1)),
    .blue = (rand() % (max + 1))
  };
  return color;
}

RGBColor rgb_random_color(int seed) {
  short max = 255;
  srand(seed);
  RGBColor color = {
    .red = (rand() % (max + 1)),
    .green = (rand() % (max + 1)),
    .blue = (rand() % (max + 1))
  };
  return color;
}

NCColor rgb2nc_color(RGBColor rgb) {
  NCColor color = {
    .red = rgb2nc(rgb.red),
    .green = rgb2nc(rgb.green),
    .blue = rgb2nc(rgb.blue)
  };
  return color;
}

RGBColor nc2rgb_color(NCColor color) {
  RGBColor rgb = {
    .red = nc2rgb(color.red),
    .green = nc2rgb(color.green),
    .blue = nc2rgb(color.blue)
  };
  return rgb;
}

NCColor nc_color_content(short color) {
  NCColor nccolor = {0};
  color_content(color, &(nccolor.red), &(nccolor.green), &(nccolor.blue));
  return nccolor;
}
