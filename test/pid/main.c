/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdio.h>
#include <string.h>

#include "pid.h"
#include "logger/logger.h"

int main(int argc, char const *argv[]) {
  if (argc < 3) return -1;

  LogInit(L_INFO, NULL, "/dev/null");

  if (strcmp(argv[1], "init") == 0) {
    pid_t mypid = PID_init(argv[2]);
    if (mypid) return 0;
  }

  if (strcmp(argv[1], "load") == 0) {
    pid_t mypid = PID_load(argv[2]);
    if (mypid) return 0;
  }

  if (strcmp(argv[1], "check") == 0) {
    PID_check(argv[2]);
    return 0;
  }

  return -1;
}
