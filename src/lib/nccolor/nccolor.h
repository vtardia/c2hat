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
