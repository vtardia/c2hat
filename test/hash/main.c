/**
 * Copyright (C) 2021 Vito Tardia
 *
 * This file is part of vHashLib.
 *
 * vHashLib is a simple C implementation of hashes
 * (associative arrays) using Hash Tables.
 *
 * vHashLib is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>

#include "hash/hash.h"
#include "hash_tests.h"

#ifndef LOCALE
#define LOCALE "en_GB.UTF-8"
#endif

int main() {
  if (strstr(LOCALE, "UTF-8") == NULL) {
    printf("The given locale (%s) does not support UTF-8\n", LOCALE);
    return EXIT_FAILURE;
  }

  if (!setlocale(LC_ALL, LOCALE)) {
    printf("Unable to set locale to '%s'\n", LOCALE);
    return EXIT_FAILURE;
  }

  printf("\nUsing locale: %s\n", LOCALE);

  printf("\n");

  printf("Basic tests\n");
  TestHash_new();
  TestHash_set();
  TestHash_first();
  TestHash_last();
  TestHash_delete();

  printf("\n");
  printf("\n");

  printf("Unicode tests\n");
  TestHash_unicode();

  printf("\n");

  printf("Unicode bulk tests\n");
  TestHash_bulk();

  printf("\n");
  printf("Done!\n\n");
  return EXIT_SUCCESS;
}
