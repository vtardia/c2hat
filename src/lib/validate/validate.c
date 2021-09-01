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

#include "validate.h"
#include <stdio.h>

int Regex_match(
  const char *subject, const char *pattern, char *error, const size_t errorSize
) {
  regex_t regex;
  int success = -1;

  // Compile the regex
  // int regcomp(regex_t *preg, const char *regex, int cflags);
  int compErrorCode = regcomp(&regex, pattern, REG_ICASE);
  if (compErrorCode) {
    // regerror() checks if buffer and buffer size are non-zero
    regerror(compErrorCode, &regex, error, errorSize);
    return success;
  }

  // Execute the regex
  // int regexec(const regex_t *preg, const char *string, size_t nmatch, regmatch_t pmatch[], int eflags);
  int execErrorCode = regexec(&regex, subject, 0, NULL, 0);
  switch (execErrorCode) {
    case 0: // Match
      success = 1;
      break;
    case REG_NOMATCH:
      success = 0;
      break;
    default:
      regerror(execErrorCode, &regex, error, errorSize);
  }

  // Free memory
  regfree(&regex);
  return success;
}

