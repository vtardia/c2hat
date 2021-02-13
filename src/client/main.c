/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "client.h"

int main(int argc, char const *argv[]) {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  if (argc < 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    return 1;
  }

  const char * host = argv[1];
  const char * port = argv[2];

  SOCKET server = Client_connect(host, port);
  if (server == -1) {
    fprintf(stderr, "Connection failed\n");
  #if defined(_WIN32)
    WSACleanup();
  #endif
    return 1;
  }

  Client_listen(server);

#if defined(_WIN32)
  WSACleanup();
#endif
  printf("Bye!\n");
  return 0;
}
