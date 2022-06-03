/*
 * Copyright (C) 2022 Vito Tardia
 */

#ifndef COMMANDS_H
#define COMMANDS_H

  #include "server.h"

  /// Command identifiers
  typedef enum Command {
    kCommandUnknown = -1,
    kCommandStart = 0,
    kCommandStop = 1,
    kCommandStatus = 2
  } Command;

  Command parseCommand(int argc, const char *arg);

  int CMD_runStart(ServerConfigInfo *settings);
  int CMD_runStop();
  int CMD_runStatus();
#endif
