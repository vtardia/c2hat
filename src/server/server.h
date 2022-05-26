/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef SERVER_H
#define SERVER_H

  #include <unistd.h>

  #define kMaxPath 4096
  #define kMaxHostLength 40
  #define kMaxLocaleLength 25

  typedef struct Server Server;

  /// Contains the server's active configuration
  typedef struct {
    pid_t pid; ///< PID for the currently running server
    unsigned int logLevel;  ///< Default log level
    char logFilePath[kMaxPath]; ///< Log file path
    char pidFilePath[kMaxPath]; ///< PID file path
    char sslCertFilePath[kMaxPath]; ///< SSL certificate file path
    char sslKeyFilePath[kMaxPath]; ///< SSL public key file path
    char host[kMaxHostLength]; ///< Listening IP address
    char locale[kMaxLocaleLength]; ///< Server locale
    unsigned int port; ///< Listening TCP port
    unsigned int maxConnections; ///< Max connections
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
#include "message/message.h"
#include "socket/socket.h"
#include "list/list.h"
#include "queue/queue.h"
#include "config/config.h"
