/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "client.h"

#include <stdlib.h>
#include <string.h>

void init() {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    exit(EXIT_FAILURE);
  }
#endif
}

int cleanup(int result) {
#if defined(_WIN32)
  WSACleanup();
#endif
  return result;
}

int main(int argc, char const *argv[]) {
  init();

  if (argc < 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    return 1;
  }

  const char * host = argv[1];
  const char * port = argv[2];

  // Create chat client
  C2HatClient *app = Client_create();
  if (app == NULL) {
    fprintf(stderr, "Client creation failed\n");
    return cleanup(EXIT_FAILURE);
  }

  // Try to connect
  if (!Client_connect(app, host, port)) {
    fprintf(stderr, "Connection failed\n");
    Client_destroy(&app);
    return cleanup(EXIT_FAILURE);
  }

  // Authenticate
  char nickname[30] = {0};
  fprintf(stdout, "Please, enter a nickname: ");
  if (!fgets(nickname, 30, stdin)) {
    fprintf(stderr, "Unable to authenticate\n");
    Client_destroy(&app);
    return cleanup(EXIT_FAILURE);
  }
  // Remove newline from nickname
  char *end = nickname + strlen(nickname) -1;
  *end = 0;
  if (!Client_authenticate(app, nickname)) {
    Client_destroy(&app);
    return cleanup(EXIT_FAILURE);
  }

  fprintf(stdout, " => To send data, enter text followed by enter.\n");
  Client_run(app, stdin, stdout, stderr);
  Client_destroy(&app);

  printf("Bye!\n");
  return cleanup(EXIT_SUCCESS);
}
