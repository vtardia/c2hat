/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef CONFIG_H
#define CONFIG_H

  #include <stdbool.h>
  #include <stdlib.h>

  // Save the given data to a shared memory location
  bool Config_save(const void *data, size_t size, const char *path);

  // Load data from a shared memory location
  void *Config_load(const char *path, size_t size);

  // Clean a shared memory location
  bool Config_clean(const char *path);

#endif
