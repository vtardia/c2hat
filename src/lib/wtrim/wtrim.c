/**
 *  This library implements trim functions for multi-byte C strings
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

#include "wtrim.h"
#include <string.h>
#include <wchar.h>

/**
 * Removes leading spaces (or other custom characters)
 * from the given string
 */
wchar_t *wltrim(wchar_t *str, const wchar_t *seps) {
  size_t totrim;
  if (seps == NULL) {
    seps = L"\t\n\v\f\r ";
  }
  // Returns the length of the maximum initial segment of the (wide) string
  // pointed to by str, that consists of only the characters found
  // in (wide) string pointed to by seps.
  totrim = wcsspn(str, seps);
  if (totrim > 0) {
    size_t len = wcslen(str);
    if (totrim == len) {
      // The given string is all made of trimmable characters
      *str = L'\0';
    } else {
      // Move the part of the string that starts at the first
      // non-trimmable character to the beginning of the string
      wmemmove(str, str + totrim, len + 1 - totrim);
    }
  }
  return str;
}

/**
 * Removes trailing spaces (or other custom characters)
 * from the given string
 */
wchar_t *wrtrim(wchar_t *str, const wchar_t *seps) {
  if (seps == NULL) {
    seps = L"\t\n\v\f\r ";
  }
  // Walk the string starting at the end and
  // replace all matching characters with \0
  size_t i = wcslen(str) - 1;
  wchar_t *p = str + i;
  while ((p - str) >= 0 && wcschr(seps, *p) != NULL) {
    *p = L'\0';
    p--;
  }
  return str;
}

/**
 * Removes leading and trailing spaces (or other custom characters)
 * from the given string
 */
wchar_t *wtrim(wchar_t *str, const wchar_t *seps) {
  return wltrim(wrtrim(str, seps), seps);
}

