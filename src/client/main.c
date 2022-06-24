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
#include <wchar.h>

#include "ui.h"
#include "client.h"
#include "wtrim/wtrim.h"
#include "app.h"

/// ARGV wrapper for options parsing
typedef char * const * ARGV;

enum {
  kMaxHostnameSize = 128,
  kMaxPortSize = 6,
  kMaxStatusMessageSize = kMaxHostnameSize + kMaxPortSize + 50,
  kMaxFilePath = 4096
};

/// Contains the client startup parameters
typedef struct _options {
  char user[kMaxNicknameSize];
  char host[kMaxHostnameSize];
  char port[kMaxPortSize];
  char caCertFilePath[kMaxFilePath];
  char caCertDirPath[kMaxFilePath];
} Options;

static const char *kC2HatClientVersion = "1.0";
static const char *kDefaultCACertFilePath = ".local/share/c2hat/ssl/cacert.pem";
static const char *kDefaultCACertDirPath = ".local/share/c2hat/ssl";

void usage(const char *program);
void help(const char *program);
void version(const char *program);
void parseOptions(int argc, ARGV argv, Options *params);

int main(int argc, ARGV argv) {
  // First check we are running in a terminal (TTY)
  if (!isatty(fileno(stdout))) {
    fprintf(stderr, "❌ Error: ENOTTY - Invalid terminal\n");
    fprintf(stderr, "Cannot start the C2Hat client in a non-interactive terminal\n");
    return EXIT_FAILURE;
  }

  // Check command line options and arguments
  Options options = {};
  parseOptions(argc, argv, &options);

  // Calling setlocale() with an empty string loads the LANG env var
  if (!setlocale(LC_ALL, "")) {
    fprintf(stderr, "Unable to read locale");
    return EXIT_FAILURE;
  }

  // Calling setlocale() with a NULL argument reads the corresponding LC_ var
  // If the system does not support the locale it will return "C"
  // otherwise the full locale string (e.g. en_US.UTF-8)
  char *locale = setlocale(LC_ALL, NULL);

  // Check locale compatibility
  if (strstr(locale, "UTF-8") == NULL) {
    fprintf(stderr, "The given locale (%s) does not support UTF-8\n", locale);
    return EXIT_FAILURE;
  }

  // To correctly display Advanced Character Set in UTF-8 environment
  setenv("NCURSES_NO_UTF8_ACS", "0", 1);

  // Initialise sockets on Windows
  App_init();

  // Create chat client
  C2HatClient *app = Client_create(options.caCertFilePath, options.caCertDirPath);
  if (app == NULL) {
    fprintf(stderr, "Chat client creation failed\n");
    return App_cleanup(EXIT_FAILURE);
  }

  // Try to connect
  if (!Client_connect(app, options.host, options.port)) {
    Client_destroy(&app);
    return App_cleanup(EXIT_FAILURE);
  }

  // Authenticate
  char nickname[kMaxNicknameSize + sizeof(wchar_t)] = {};

  if (strlen(options.user) > 0) {
    // Use the nickname provided from the command line...
    strncpy(nickname, options.user, kMaxNicknameSize);
  } else {
    // ...or read from the input as Unicode (UCS)
    wchar_t inputNickname[kMaxNicknameInputBuffer] = {};
    fprintf(stdout, "   〉Please, enter a nickname (max %d chars): ", kMaxNicknameLength);
    fflush(stdout); // or some clients won't display the above
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
  char connectionStatus[kMaxStatusMessageSize] = {};
  int statusMessageLength = sprintf(
      connectionStatus, "Connected to %s:%s - Hit F1 to quit",
      options.host, options.port
  );
  UISetStatusMessage(connectionStatus, statusMessageLength);

  // Set up event handlers
  App_catch(SIGINT, App_terminate);
  App_catch(SIGTERM, App_terminate);
  App_catch(SIGWINCH, UIResizeHandler);
  App_catch(SIGUSR1, UITerminate);
  App_catch(SIGUSR2, UIQueueHandler);

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
 * Displays program version
 */
void version(const char *program) {
  fprintf(
    stderr, "%1$s - C2Hat client [version %2$s]\n",
    basename((char *)program), kC2HatClientVersion
  );
}

/**
 * Displays program usage
 */
void usage(const char *program) {
  fprintf(stderr,
    "Usage: %1$s [options] <host> <port>\n"
    "       %1$s [-u YourNickname] <host> <port>\n"
    "\n"
    "For a listing of options, use %1$s --help."
    "\n", basename((char *)program)
  );
}

/**
 * Displays program help
 */
void help(const char *program) {
  fprintf(stderr,
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
    "   -u, --user      specify a user's nickname before connecting;\n"
    "       --cacert    specify a CA certificate to verify with;\n"
    "       --capath    specify a directory where trusted CA certificates\n"
    "                   are stored; if neither cacert and capath are\n"
    "                   specified, the default path will be used:\n"
    "                   $HOME/.local/share/c2hat/ssl\n"
    "   -v, --version   display the current program version;\n"
    "   -h, --help      display this help message;\n"
    "\n", basename((char *)program), kC2HatClientVersion
  );
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
    {"cacert", required_argument, NULL, 'f'},
    {"capath", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    { NULL, 0, NULL, 0}
  };

  // Setup default SSL config
  snprintf(params->caCertFilePath, kMaxFilePath - 1, "%s/%s", getenv("HOME"), kDefaultCACertFilePath);
  snprintf(params->caCertDirPath, kMaxFilePath - 1, "%s/%s", getenv("HOME"), kDefaultCACertDirPath);

  // Parse the command line arguments into options
  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "u:hv", options, NULL);
    if( (signed char)ch == -1 ) break; // No more options available
    switch (ch) {
      case 'h': // User requested help, display it and exit
        help(argv[0]);
        exit(EXIT_SUCCESS);
      break;
      case 'v': // User requested version, display it and exit
        version(argv[0]);
        exit(EXIT_SUCCESS);
      break;
      case 'u': // User passed a nickname
        strncpy(params->user, optarg, kMaxNicknameSize - 1);
      break;
      case 'f': // User passed a CA certificate file
        strncpy(params->caCertFilePath, optarg, kMaxFilePath - 1);
      break;
      case 'd': // User passed a CA directory path
        strncpy(params->caCertDirPath, optarg, kMaxFilePath - 1);
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

