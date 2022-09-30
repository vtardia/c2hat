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

#ifndef TRIM_H
#define TRIM_H

  /**
   * Removes leading spaces (or other custom characters)
   * from the given string
   */
  char *ltrim(char *str, const char *seps);

  /**
   * Removes trailing spaces (or other custom characters)
   * from the given string
   */
  char *rtrim(char *str, const char *seps);

  /**
   * Removes leading and trailing spaces (or other custom characters)
   * from the given string
   */
  char *trim(char *str, const char *seps);
#endif

