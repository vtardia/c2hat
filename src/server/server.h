/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef SERVER_H
#define SERVER_H

  typedef struct Server Server;

  // Create a server for the given host/port combination
  Server *Server_init(const char *host, const int port, const int maxConnections);

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
#include "message.h"
#include "socket/socket.h"
#include "list/list.h"
#include "queue/queue.h"
