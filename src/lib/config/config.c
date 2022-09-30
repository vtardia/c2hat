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

#include "config.h"

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>

/**
 * Creates a shared memory and copy the configuration there
 * @param[in] data The configuration data to save
 * @param[in] size The size of data to save
 * @param[in] path The name of the shared memory location, starting with '/''
 */
bool Config_save(const void *data, size_t size, const char *path) {

  // Create the shared memory object
  int fd = shm_open(path, O_CREAT | O_RDWR | O_TRUNC, 0600);
  if (fd < 0) {
    return false;
  }

  // Set the size
  if (ftruncate(fd, size) < 0) {
    return false;
  }

  // Connect the memory to a local pointer
  void* map = mmap(0, size, PROT_WRITE, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    return false;
  }

  // Transfer data into the memory
  memcpy(map, data, size);

  // Cleanup and close
  if (munmap(map, size) < 0) {
    return false;
  }
  if (close(fd) < 0) {
    return false;
  }
  return true;
}

/**
 * Loads configuration data from the given shared memory location
 * On failure NULL is returned and errno is set by the shm_* functions
 * On success the returned object needs to be freed
 * @param[in] path The name of the shared memory location, starting with '/''
 * @param[in] size The size of data to save
 */
void *Config_load(const char *path, size_t size) {
  // Open and map the shared memory location
  int fd = shm_open(path, O_RDONLY, 0600);
  if (fd < 0) {
    return NULL;
  }
  void* map = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
  if (map == MAP_FAILED) {
    return NULL;
  }

  // Copy the data
  void *data = calloc(size, 1);
  memcpy(data, map, size);

  // Close the shared memory
  if (munmap(map, size) < 0) {
    return NULL;
  }
  if (close(fd) < 0) {
    return NULL;
  }
  return data;
}

/**
 * Removes configuration data from the given shared memory path
 * @param[in] path The name of the shared memory location, starting with '/''
 */
bool Config_clean(const char *path) {
  if (shm_unlink(path) < 0) {
    return false;
  }
  return true;
}
