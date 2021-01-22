#ifndef CLIENT_H
#define CLIENT_H
  #include "socket.h"

  SOCKET Client_connect(const char *host, const char *port);

  void Client_listen(SOCKET server);
#endif

#include <stdio.h>
