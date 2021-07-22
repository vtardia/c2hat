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

#ifndef WTRIM_H
#define WTRIM_H
  #include <stddef.h>

  /**
   * Removes leading spaces (or other custom characters)
   * from the given string
   */
  wchar_t *wltrim(wchar_t *str, const wchar_t *seps);

  /**
   * Removes trailing spaces (or other custom characters)
   * from the given string
   */
  wchar_t *wrtrim(wchar_t *str, const wchar_t *seps);

  /**
   * Removes leading and trailing spaces (or other custom characters)
   * from the given string
   */
  wchar_t *wtrim(wchar_t *str, const wchar_t *seps);
#endif

