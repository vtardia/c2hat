/**
 *  This library implements trim functions for standard C strings
 *
 *  It contains code from Martin Broadhurst,
 *  see http://www.martinbroadhurst.com/trim-a-string-in-c.html
 *
 *  Copyright (C) 2021  Vito Tardia
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "trim.h"

#include <string.h>

/**
 * Removes leading spaces (or other custom characters)
 * from the given string
 */
char *ltrim(char *str, const char *seps) {
  size_t totrim;
  if (seps == NULL) {
    seps = "\t\n\v\f\r ";
  }
  // Returns the length of the maximum initial segment of the string
  // pointed to by str, that consists of only the characters found
  // in string pointed to by seps.
  totrim = strspn(str, seps);
  if (totrim > 0) {
    size_t len = strlen(str);
    if (totrim == len) {
      // The given string is all made of trimmable characters
      str[0] = '\0';
    } else {
      // Move the part of the string that starts at the first
      // non-trimmable character to the beginning of the string
      memmove(str, str + totrim, len + 1 - totrim);
    }
  }
  return str;
}

/**
 * Removes trailing spaces (or other custom characters)
 * from the given string
 */
char *rtrim(char *str, const char *seps) {
  int i;
  if (seps == NULL) {
    seps = "\t\n\v\f\r ";
  }
  // Walk the string starting at the end and
  // replace all matching characters with \0
  i = strlen(str) - 1;
  while (i >= 0 && strchr(seps, str[i]) != NULL) {
    str[i] = '\0';
    i--;
  }
  return str;
}

/**
 * Removes leading and trailing spaces (or other custom characters)
 * from the given string
 */
char *trim(char *str, const char *seps) {
  return ltrim(rtrim(str, seps), seps);
}

