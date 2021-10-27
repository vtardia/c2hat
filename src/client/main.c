/*
 * Copyright (C) 2021 Vito Tardia
 *
 * This file is the main entry point of the C2Hat CLI client
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <locale.h>
#include <getopt.h>
#include <libgen.h>

#include "ui.h"
#include "client.h"
#include "trim/wtrim.h"
#include "app.h"

#ifndef LOCALE
#define LOCALE "en_GB.UTF-8"
#endif

/// ARGV wrapper for options parsing
typedef char * const * ARGV;

enum {
  kMaxHostnameSize = 128,
  kMaxPortSize = 6,
  kMaxStatusMessageSize = kMaxHostnameSize + kMaxPortSize + 50
};

/// Contains the client startup parameters
typedef struct _options {
  char user[kMaxNicknameSize];
  char host[kMaxHostnameSize];
  char port[kMaxPortSize];
} Options;

void usage(const char *program);
void parseOptions(int argc, ARGV argv, Options *params);

int main(int argc, ARGV argv) {
  // First check we are running in a terminal (TTY)
  if (!isatty(fileno(stdout))) {
    fprintf(stderr, "❌ Error: ENOTTY - Invalid terminal\n");
    fprintf(stderr, "Cannot start the C2Hat client in a non-interactive terminal\n");
    return EXIT_FAILURE;
  }

  // Check command line options and arguments
  Options options = {0};
  parseOptions(argc, argv, &options);

  // Check locale compatibility
  if (strstr(LOCALE, "UTF-8") == NULL) {
    fprintf(stderr, "The given locale (%s) does not support UTF-8\n", LOCALE);
    return EXIT_FAILURE;
  }

  if (!setlocale(LC_ALL, LOCALE)) {
    fprintf(stderr, "Unable to set locale to '%s'\n", LOCALE);
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
  if (!Client_connect(app, options.host, options.port)) {
    fprintf(stderr, "Connection failed\n");
    Client_destroy(&app);
    return App_cleanup(EXIT_FAILURE);
  }

  // Authenticate
  char nickname[kMaxNicknameSize + sizeof(wchar_t)] = {0};

  if (strlen(options.user) > 0) {
    // Use the nickname provided from the command line...
    strncpy(nickname, options.user, kMaxNicknameSize);
  } else {
    // ...or read from the input as Unicode (UCS)
    wchar_t inputNickname[kMaxNicknameInputBuffer] = {0};
    fprintf(stdout, "Please, enter a nickname (max 12 chars): ");
    // fgetws() reads length -1 characters and includes the new line
    if (!fgetws(inputNickname, kMaxNicknameInputBuffer, stdin)) {
      fprintf(stderr, "Unable to read nickname\n");
      Client_destroy(&app);
      return App_cleanup(EXIT_FAILURE);
    }

    // Remove unwanted trailing spaces and new line characters
    wchar_t *trimmedNickname = wtrim(inputNickname, NULL);

    // Convert into UTF-8
    wcstombs(nickname, trimmedNickname, kMaxNicknameSize + sizeof(wchar_t));
  }

  // Send to the server for authentication
  if (!Client_authenticate(app, nickname)) {
    Client_destroy(&app);
    return App_cleanup(EXIT_FAILURE);
  }

  // Initialise NCurses UI engine
  UIInit();
  char connectionStatus[kMaxStatusMessageSize] = {0};
  int statusMessageLength = sprintf(
      connectionStatus,
      "Connected to %s:%s - Hit F1 to quit",
      options.host,
      options.port
  );
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
  fprintf(stdout, "Disconnecting...");
  pthread_join(listeningThreadID, NULL);

  // Clean Exit
  Client_destroy(&app);
  fprintf(stdout, "Bye!\n");
  return App_cleanup(EXIT_SUCCESS);
}

/**
 * Displays program usage
 */
void usage(const char *program) {
  const char *version = "1.0";
  FILE *target = stderr;
  fprintf(target,
"%1$s - commandline C2Hat client [version %2$s]\n"
"\n"
"Usage: %1$s [options] <host> <port>\n"
"       %1$s [-u YourNickname] <host> <port>\n"
"\n"
"%1$s is a commandline ncurses-based client for the C2Hat server\n"
"platform.\n"
"\n"
"It provides an interactive chat environment to send and receive\n"
"messages up to 280 Unicode characters, including emojis.\n"
"\n"
"Examples:\n"
"\n"
"   $ %1$s chat.example.com 10000\n"
"   $ %1$s -u Uncl3Ozzy chat.example.com 10000\n"
"\n"
"Current options include:\n"
"   -u    specify a user's nickname before connecting;\n"
"\n", basename((char *)program), version);
}

/**
 * Parses command line options
 * @param[in] argc The number of arguments
 * @param[in] argv The array of arguments
 * @param[in] options Pointer to a configuration structure
 */
void parseOptions(int argc, ARGV argv, Options *params) {
  // Check that we have the minimum required command line args
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // Build the options list
  struct option options[] = {
    {"user", required_argument, NULL, 'u'},
    {"help", no_argument, NULL, 'h'},
    { NULL, 0, NULL, 0}
  };

  // Parse the command line arguments into options
  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "u:h", options, NULL);
    if( (signed char)ch == -1 ) break; // No more options available
    switch (ch) {
      case 'h': // User requested help, display it and exit
        usage(argv[0]);
        exit(EXIT_SUCCESS);
      break;
      case 'u': // User passed a nickname
        strncpy(params->user, optarg, kMaxNicknameSize - 1);
      break;
    }
  }

  // We need at least 2 arguments left: host and port
  if ((argc - optind) < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }
  // Copy host and port into the options struct
  strncpy(params->host, argv[optind++], kMaxHostnameSize - 1);
  strncpy(params->port, argv[optind], kMaxPortSize - 1);
}

