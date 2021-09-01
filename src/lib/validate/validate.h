/**
 *  This library implements data validation for C2Hat client/server
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

#ifndef VALIDATE_H
#define VALIDATE_H

  #include <sys/types.h>
  #include <stdbool.h>
  #include <regex.h>

  /**
   * General purpose regex string match function
   * Returns the Regex parse/execute error string into the third parameter
   * @param[in]  subject   The string to check
   * @param[in]  pattern   The regular expression pattern used for validation
   * @param[in]  error     Pointer to a buffer where to store the error
   * @param[in]  errorSize Length of the error buffer
   * @param[out]           1 => match, 0 => no match, -1 => other regex error
   */
  int Regex_match(
    const char *subject, const char *pattern, char *error, const size_t errorSize
  );

#endif

