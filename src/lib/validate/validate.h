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

