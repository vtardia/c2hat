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

#ifndef _V_FS_UTIL_H_
#define _V_FS_UTIL_H_
  #include <stdbool.h>
  #include <sys/stat.h>
  #include <unistd.h>

  /**
   * Checks if the given directory exists and creates recursively
   * if it doesn't exist.
   *
   * Returns true if either the directory already exists or
   * it's created successfully.
   *
   * On error returns false and an error code is stored in errno.
   *
   * Returns false if one of the path component does not exist
   * or it's not a directory (ENOENT).
   */
  bool TouchDir(const char *path, mode_t mode);

  /**
   * Checks if the given path is a directory
   */
  bool IsDir(const char *path);

  /**
   * Checks if the given path is a file
   */
  bool IsFile(const char *path);

  /**
   * Checks if the given path is a symbolic link
   */
  bool IsLink(const char *path);

  /**
   * Checks if the given path is a FIFO
   */
  bool IsFiFo(const char *path);

  /**
   * Checks if the given path is a socket
   */
  bool IsSocket(const char *path);

  /**
   * Checks if a file or directory exists
   */
  #define Exists(path) (access(path, F_OK) == 0)

  /**
   * Checks if a file or directory is readable
   */
  #define IsReadable(path) (access(path, R_OK) == 0)

  /**
   * Checks if a file or directory is writeable
   */
  #define IsWritable(path) (access(path, W_OK) == 0)
#endif

