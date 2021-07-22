/*
 * Copyright (C) 2021 Vito Tardia
 *
 * This file is the main entry point of the C2Hat CLI client
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <locale.h>

#include "ui.h"
#include "client.h"
#include "trim/wtrim.h"
#include "app.h"

#ifndef LOCALE
#define LOCALE "en_GB.UTF-8"
#endif

int main(int argc, char const *argv[]) {
  // Validate command line arguments
  if (argc < 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    return 1;
  }
  const char * host = argv[1];
  const char * port = argv[2];

  // Check locale compatibility
  if (strstr(LOCALE, "UTF-8") == NULL) {
    printf("The given locale (%s) does not support UTF-8\n", LOCALE);
    return EXIT_FAILURE;
  }

  if (!setlocale(LC_ALL, LOCALE)) {
    printf("Unable to set locale to '%s'\n", LOCALE);
    return EXIT_FAILURE;
  }
  setenv("NCURSES_NO_UTF8_ACS", "0", 1);

  // Initialise sockets on Windows
  App_init();

  // Create chat client
  C2HatClient *app = Client_create();
  if (app == NULL) {
    fprintf(stderr, "Client creation failed\n");
    return App_cleanup(EXIT_FAILURE);
  }

  // Try to connect
  if (!Client_connect(app, host, port)) {
    fprintf(stderr, "Connection failed\n");
    Client_destroy(&app);
    return App_cleanup(EXIT_FAILURE);
  }

  // Authenticate
  // Read from the input as Unicode (UCS)
  // Account for the extra new line char and null terminator
  // and that some emoji have more bytes to fill
  wchar_t inputNickname[kMaxNicknameLength + 3] = {0};
  fprintf(stdout, "Please, enter a nickname (max 12 chars): ");
  // fgetws() reads length -1 characters and includes the new line
  // so we add +3 to take into account this and emoji size
  if (!fgetws(inputNickname, kMaxNicknameLength + 3, stdin)) {
    fprintf(stderr, "Unable to authenticate\n");
    Client_destroy(&app);
    return App_cleanup(EXIT_FAILURE);
  }
  // Remove unwanted trailing spaces and new line characters
  wchar_t *trimmedNickname = wtrim(inputNickname, NULL);

  // Convert into UTF-8
  char nickname[kMaxNicknameSize + 1 * sizeof(wchar_t)] = {0};
  wcstombs(nickname, trimmedNickname, kMaxNicknameSize + 1 * sizeof(wchar_t));
  // Send to the server for authentication
  if (!Client_authenticate(app, nickname)) {
    Client_destroy(&app);
    return App_cleanup(EXIT_FAILURE);
  }

  // Initialise NCurses UI engine
  UIInit();
  char connectionStatus[120] = {0};
  int statusMessageLength = sprintf(connectionStatus, "Connected to %s:%s - Hit F1 to quit", host, port);
  UISetStatusMessage(connectionStatus, statusMessageLength);

  // Set up event handlers
  App_catch(SIGINT, App_terminate);
  App_catch(SIGTERM, App_terminate);
  App_catch(SIGWINCH, UIResizeHandler);

  // Start a new thread that listens for messages from the server
  // and updates the chat log window
  pthread_t listeningThreadID = 0;
  pthread_create(&listeningThreadID, NULL, App_listen, app);

  // Start the app infinite loop
  App_run(app);

  // Cleanup UI
  UIClean();

  // Wait for the listening thread to finish
  fprintf(stdout, "Disconnecting...\n");
  pthread_join(listeningThreadID, NULL);

  // Clean Exit
  Client_destroy(&app);
  fprintf(stdout, "Bye!\n");
  return App_cleanup(EXIT_SUCCESS);
}
