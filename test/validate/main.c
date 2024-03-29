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

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "validate/validate.h"

/**
 * Username validation mattern: "^[[:alpha:]][[:alnum:]!@#$%&]{1,14}$"
 *  - must start with a letter
 *  - min 2 max 15 characters
 *  - only alphanumeric latin characters and !@#$%&
 */
char *usernamePattern = "^[[:alpha:]][[:alnum:]!@#$%&]\\{1,14\\}$";

int main() {

  // Cannot be shorter than 2 characters
  assert(!Regex_match("J", usernamePattern, NULL, 0));
  printf(".");

  assert(Regex_match("Jo", usernamePattern, NULL, 0));
  printf(".");

  // Cannot be longer than 15 characters
  assert(!Regex_match("UsernameLongerThan15Characters", usernamePattern, NULL, 0));
  printf(".");

  assert(Regex_match("UsernameWith15C", usernamePattern, NULL, 0));
  printf(".");

  assert(Regex_match("J0e$m1th99", usernamePattern, NULL, 0));
  printf(".");

  // Must start with a letter
  assert(!Regex_match("10Endians", usernamePattern, NULL, 0));
  printf(".");

  assert(!Regex_match("@SomeOne", usernamePattern, NULL, 0));
  printf(".");

  // No emojis
  assert(!Regex_match("Hallo🎃", usernamePattern, NULL, 0));
  printf(".");

  assert(!Regex_match("🎃Hallo", usernamePattern, NULL, 0));
  printf(".");

  assert(!Regex_match("Ha🎃llo", usernamePattern, NULL, 0));
  printf(".");

  assert(!Regex_match("🎃🍻🤦🏻", usernamePattern, NULL, 0));
  printf(".");

  // No spaces allowed
  assert(!Regex_match("No Spaces", usernamePattern, NULL, 0));
  printf(".");

  // Only latin characters and !@#$%&
  assert(Regex_match("Holy!@#$%&", usernamePattern, NULL, 0));
  printf(".");

  assert(!Regex_match("Holy!@#$%&^;", usernamePattern, NULL, 0));
  printf(".");

  // Cannot contain newline characters
  assert(!Regex_match("Holy\nJoeBlog", usernamePattern, NULL, 0));
  printf(".");

  printf("\n");
  return EXIT_SUCCESS;
}

