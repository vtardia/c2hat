/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "client.h"

#include <stdlib.h>

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

  // FILE *chatlog = fopen("/tmp/c2hatclient.log", "a");

  C2HatClient *app = Client_create(host, port);
  if (app == NULL) {
    fprintf(stderr, "Connection failed\n");
  #if defined(_WIN32)
    WSACleanup();
  #endif
    return EXIT_FAILURE;
  }

  Client_run(app, stdin, stdout, stderr);

  Client_destroy(&app);

#if defined(_WIN32)
  WSACleanup();
#endif
  printf("Bye!\n");
  // fclose(chatlog);
  return EXIT_SUCCESS;
}
