/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef CLIENT_H
#define CLIENT_H
  #include "socket/socket.h"

  SOCKET Client_connect(const char *host, const char *port);

  void Client_listen(SOCKET server);
#endif

#include <stdio.h>
