#ifndef SERVER_H
#define SERVER_H

  #include "socket.h"
  #include "logger.h"

  // Create a server for the given host/port combination
  SOCKET Server_new(const char *host, const int port, const int maxConnections);

  // Start the server on the given socket
  void Server_start(SOCKET);

#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>

#include "pid.h"
