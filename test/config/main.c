/*
 * Copyright (C) 2020 Vito Tardia
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
