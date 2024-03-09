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

#ifndef SERVER_H
#define SERVER_H

  #include <unistd.h>
  #include <stdbool.h>
  #include "../c2hat.h"

  typedef struct Server Server;

  /// Contains the server's active configuration
  typedef struct {
    pid_t pid; ///< PID for the currently running server
    unsigned int logLevel;  ///< Default log level
    char configFilePath[kMaxPath]; ///< Config file path, if found
    char logFilePath[kMaxPath]; ///< Log file path
    char pidFilePath[kMaxPath]; ///< PID file path
    char sslCertFilePath[kMaxPath]; ///< SSL certificate file path
    char sslKeyFilePath[kMaxPath]; ///< SSL public key file path
    char host[kMaxHostLength]; ///< Listening IP address
    char locale[kMaxLocaleLength]; ///< Server locale
    unsigned int port; ///< Listening TCP port
    unsigned int maxConnections; ///< Max connections
    bool foreground; ///< Foreground or background service flag
    char workingDirPath[kMaxPath]; ///< Server work directory
    char usersDbFilePath[kMaxPath]; ///< Users database file path
  } ServerConfigInfo;

  // Create a server for the given host/port combination
  Server *Server_init(ServerConfigInfo *);

  // Start the server on the given socket
  void Server_start(Server *);

  // Cleanup server memory
  void Server_free(Server **);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include "logger/logger.h"
#include "pid.h"
#include "config/config.h"
