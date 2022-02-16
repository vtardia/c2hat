/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef SERVER_H
#define SERVER_H

  #include <unistd.h>

  typedef struct Server Server;

  /// Contains the server's active configuration
  typedef struct {
    pid_t pid; ///< PID for the currently running server
    char logFilePath[4096]; ///< Log file path
    char pidFilePath[4096]; ///< PID file path
    char sslCertFilePath[4096]; ///< SSL certificate file path
    char sslKeyFilePath[4096]; ///< SSL public key file path
    char host[40]; ///< Listening IP address
    char locale[25]; ///< Server locale
    unsigned int port; ///< Listening TCP port
    unsigned int maxConnections; ///< Max connections
  } ServerConfigInfo;

  // Create a server for the given host/port combination
  Server *Server_init(
    const char *host, const int port, const int maxConnections,
    const char *sslCertFile, const char *sslKeyFile
  );

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
#include "message/message.h"
#include "socket/socket.h"
#include "list/list.h"
#include "queue/queue.h"
#include "config/config.h"
