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

#include "fsutil.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

bool TouchDir(const char *path, mode_t mode) {
  // Copy the input into a read/write buffer
  char buffer[512] = {};
  snprintf(buffer, sizeof(buffer), "%s", path);
  size_t pathLength = strlen(buffer);

  // Used as cursor to walk the path
  char *p = NULL;

  // Remove trailing slash if present
  if (buffer[pathLength - 1] == '/') buffer[pathLength - 1] = 0;

  if (!Exists(buffer)) {
    // Starting from the second character to avoid removing the leading '/'
    for (p = buffer + 1; *p; p++) {
      // Create intermediate components if missing
      if (*p == '/') {
        // Temporarily truncate the string at the component end
        *p = 0;
        // Try to create the partial path
        if (!Exists(buffer)) {
          if (mkdir(buffer, mode) < 0) {
            return false;
          }
        }
        // Restore the separator and continue
        *p = '/';
      }
    }
    // Create the last component
    if (mkdir(buffer, mode) < 0) {
      return false;
    }
  }
  // The directory already exist
  return true;
}

bool IsDir(const char *path) {
  struct stat st = {};
  return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

bool IsFile(const char *path) {
  struct stat st = {};
  return (stat(path, &st) == 0 && S_ISREG(st.st_mode));
}

bool IsLink(const char *path) {
  struct stat st = {};
  return (stat(path, &st) == 0 && S_ISLNK(st.st_mode));
}

bool IsFiFo(const char *path) {
  struct stat st = {};
  return (stat(path, &st) == 0 && S_ISFIFO(st.st_mode));
}

bool IsSocket(const char *path) {
  struct stat st = {};
  return (stat(path, &st) == 0 && S_ISSOCK(st.st_mode));
}

/**
 * Also available but not implemented
 *  - S_ISCHR() — character device
 *  - S_ISBLK() — block device
 */

#ifdef Test_operations
  #include <assert.h>
  #include <errno.h>
  int main(int argc, char const *argv[]) {
    (void)argc;
    char *testPath = "/tmp/fs-util/test";

    if (Exists(testPath) && IsDir(testPath)) {
      remove(testPath);
    }

    assert(IsDir("/tmp") && IsWritable("/tmp"));
    printf(".");

    assert(!IsDir(testPath));
    printf(".");

    assert(TouchDir(testPath, 0755));
    printf(".");

    assert(IsDir(testPath) && IsReadable(testPath) && IsWritable(testPath));
    printf(".");

    assert(IsFile(argv[0]));
    printf(".");

    assert(!IsFile("/tmp/foobar"));
    printf(".");

    assert(!IsReadable("/root") && !IsWritable("/root"));
    printf(".");

    printf("\n");
    return EXIT_SUCCESS;
  }
#endif

