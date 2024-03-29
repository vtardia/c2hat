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
