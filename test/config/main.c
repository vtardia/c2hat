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
#include <assert.h>
#include <string.h>

#include "config/config.h"

const char *kSharedMemoryPath = "/cfgtest";

typedef struct {
  char stringVal[1024];
  int  intVal;
} ConfigInfo;

int main() {

  ConfigInfo data = {
    .stringVal = "Some string value",
    .intVal = 42
  };
  // Test Config_save
  assert(Config_save(&data, sizeof(ConfigInfo), kSharedMemoryPath));
  printf(".");

  // Test Config_load
  ConfigInfo *loadedData = (ConfigInfo *)Config_load(kSharedMemoryPath, sizeof(ConfigInfo));
  assert(loadedData != NULL);
  printf(".");

  assert(loadedData->intVal == data.intVal);
  printf(".");

  assert(strcmp(loadedData->stringVal, data.stringVal) == 0);
  printf(".");
  memset(loadedData, 0, sizeof(ConfigInfo));
  free(loadedData);
  loadedData = NULL;

  // Test Config_clean
  assert(Config_clean(kSharedMemoryPath));
  printf(".");

  loadedData = (ConfigInfo *)Config_load(kSharedMemoryPath, sizeof(ConfigInfo));
  assert(loadedData == NULL);
  printf(".");

  printf("\n");
  return 0;
}
