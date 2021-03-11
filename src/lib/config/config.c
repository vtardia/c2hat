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
  int fd = shm_open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
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
  int fd = shm_open(path, O_RDONLY, 0644);
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
