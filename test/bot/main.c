/**
 * Copyright (C) 2020-2022 Vito Tardia <https://vito.tardia.me>
 *
 * This file is part of C2Hat
 *
 * C2Hat is a simple client/server TCP chat written in C
 *
 * C2Hat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * C2Hat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with C2Hat. If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include <libgen.h>

#include "client.h"
#include "message/message.h"
#include "logger/logger.h"
#include "fsutil/fsutil.h"

/// ARGV wrapper for options parsing
typedef char * const * ARGV;

enum {
  kMaxBots = 7,
  kMaxMessages = 100
};

/// Contains the client startup parameters
typedef struct {
  size_t maxBots;
  char   host[kMaxHostnameSize];
  char   port[kMaxPortSize];
  char   caCertFilePath[kMaxPath];
  char   caCertDirPath[kMaxPath];
} BotOptions;

static const char *kDefaultCACertFilePath = ".local/share/c2hat/ssl/cacert.pem";
static const char *kDefaultCACertDirPath = ".local/share/c2hat/ssl";

static bool terminate = false;

BotOptions options = {};
ClientOptions clientOptions = { .logLevel = LOG_INFO };

char messages[kBufferSize][kMaxMessages] = {};

void usage(const char *program);
void help(const char *program);
void parseOptions(int argc, ARGV argv, BotOptions *params);

/**
 * Called within a thread function, hides that thread from signals
 */
void maskSignal() {
  sigset_t mask;
  sigemptyset(&mask);
#if defined(__linux__)
  sigaddset(&mask, SIGRTMIN+3);
#endif
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void Bot_stop(int signal) {
  terminate = true;
  printf(
    "Received signal %d in thread %lu\n",
    signal, (unsigned long)pthread_self()
  );
}

// Catch interrupt signals
int Bot_catch(int sig, void (*handler)(int)) {
   struct sigaction action = {
     .sa_handler = handler,
     .sa_flags = 0
   };
   sigemptyset(&action.sa_mask);
   return sigaction (sig, &action, NULL);
}

void* RunBot(void* data) {
  int *id = (int *)data;

  // Create a chat client
  C2HatClient *bot = Client_create(&clientOptions);
  if (bot == NULL) {
    fprintf(stderr, "[%d] Bot client creation failed\n", *id);
    return NULL;
  }

  // Try to connect
  if (!Client_connect(bot, options.host, options.port)) {
    fprintf(stderr, "[%d] Connection failed\n", *id);
    Client_destroy(&bot);
    return NULL;
  }

  // Choose your nickname
  char nickname[kMaxNicknameSize] = {};
  sprintf(nickname, "Bot@%d", *id);

  printf("Starting Bot thread %d: %lu\n", *id, (unsigned long)pthread_self());

  // Authenticate
  if (!Client_authenticate(bot, nickname)) {
    fprintf(stderr, "[%s] Authentication failed\n", nickname);
    Client_destroy(&bot);
    return NULL;
  }

  // Initialise random engine
  srand(time(NULL));

  // Prepare for the message loop
  fd_set reads;
  FD_ZERO(&reads);
  SOCKET server = Client_getSocket(bot);
  FD_SET(server, &reads);

  // Set connection timeout
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0; // micro seconds

  // Start the message loop
  while (!terminate) {
    if (!SOCKET_isValid(server)) break;
    int result = select(server + 1, &reads, 0, 0, &timeout);
    if (result < 0) {
      if (SOCKET_getErrorNumber() == EINTR) break; // Signal received before timeout
      fprintf(
        stderr, "[%s] select() failed. (%d): %s\n",
        nickname, SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      break;
    }

    // Server didn't respond on time
    if (result == 0) {
      fprintf(
        stderr, "[%s] select() timeout expired. (%d): %s\n",
        nickname, SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      break;
    }

    if (FD_ISSET(server, &reads)) {
      // We have data in a socket
      int received = Client_receive(bot);
      if (received <= 0) {
        break;
      }
      // Print up to byte_received from the server
      MessageBuffer *buffer = Client_getBuffer(bot);
      printf("[%s/server]: %.*s\n", nickname, received, buffer->start);
    }

    // Throw a dice to send a message
    int probability = rand() % 100;
    if (probability < 30) {
      int messageID = rand() % 100;
      char message[kBufferSize] = {};
      snprintf(message, sizeof(message) -1, "/msg %s", messages[messageID]);
      int sent = Client_send(bot, message, strlen(message) + 1);
      if (sent <= 0) {
        fprintf(
          stderr, "[%s] Unable to send message: %s\n",
          nickname, strerror(errno)
        );
      }
    }
    sleep(1);
  }

  // Signal received, prepare to close
  printf("[%s] Closing connection...\n", nickname);
  int sent = Client_send(bot, "/quit", strlen("/quit") + 1);
  if (sent <= 0) {
    fprintf(
      stderr, "[%s] Unable close connection: %s\n",
      nickname, strerror(errno)
    );
  }
  FD_CLR(server, &reads);
  Client_destroy(&bot);
  return id;
}

/// Load the test messages from the file
void LoadMessages() {
  FILE *fd = fopen("test/bot/messages.txt", "r");
  if (!fd) {
    fprintf(stderr, "Unable to open messages file: %s\n", strerror(errno));
    exit(1);
  }
  for (int i = 0; i < kMaxMessages; ++i) {
    memset(messages[i], 0, sizeof(messages[i]));
    fgets(messages[i], sizeof(messages[i]) -1, fd);
    // Remove last new line character
    *(messages[i] + strlen(messages[i]) -1) = 0;
  }
  fclose(fd);
}

/// Initialise sockets (win only)
void BotInit() {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    exit(EXIT_FAILURE);
  }
#endif
}

/// Cleanup sockets and return (win only)
int BotCleanup(int result) {
#if defined(_WIN32)
  WSACleanup();
#endif
  return result;
}

int main(int argc, ARGV argv) {
  // Check command line options and arguments
  parseOptions(argc, argv, &options);

  BotInit();

  LoadMessages();

  Bot_catch(SIGINT, Bot_stop);
  Bot_catch(SIGTERM, Bot_stop);

  pthread_t threadId[options.maxBots];
  int values[options.maxBots];

  // Start new threads to process, each with a different input value
  for(size_t i = 0; i < options.maxBots; i++) {
    values[i] = i;
    pthread_create(&threadId[i], NULL, RunBot, &values[i]);
  }

  printf("Main loop... %lu\n", (unsigned long)pthread_self());

  // Wait for all the threads to finish and close.
  // Note: if some threads are already terminated (e.g. connection denied),
  // trying to join them by passing a pointer causes a segmentation fault.
  for(size_t j = 0; j < options.maxBots; j++) {
    int res = pthread_join(threadId[j], NULL);
    switch(res) {
      case 0:
        printf("Bot %ld joined!\n", j);
      break;
      case EINVAL:
        fprintf(stderr, "Unable to join Bot %ld: thread not join able\n", j);
      break;
      case ESRCH:
        fprintf(stderr, "Unable to join Bot %ld: thread not found\n", j);
      break;
      case EDEADLK:
        fprintf(stderr, "Unable to join Bot %ld: possible deadlock\n", j);
      break;
      default:
        fprintf(stderr, "Unable to join Bot %ld: %s\n", j, strerror(errno));
      break;
    }
  }

  printf("Terminating...\n");
  printf("Bye!\n");
  return BotCleanup(EXIT_SUCCESS);
}

/**
 * Displays program usage
 */
void usage(const char *program) {
  fprintf(stderr,
    "Usage: %1$s [options] <host> <port>\n"
    "       %1$s [-n HowManyBots] <host> <port>\n"
    "\n"
    "For a listing of options, use %1$s --help."
    "\n", basename((char *)program)
  );
}

/**
 * Parses command line options
 * @param[in] argc The number of arguments
 * @param[in] argv The array of arguments
 * @param[in] options Pointer to a configuration structure
 */
void parseOptions(int argc, ARGV argv, BotOptions *params) {
  // Check that we have the minimum required command line args
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // Build the options list
  int debug = 0;
  struct option options[] = {
    {"num-bots", required_argument, NULL, 'n'},
    {"cacert", required_argument, NULL, 'f'},
    {"capath", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"debug", no_argument, &debug, 1},
    { NULL, 0, NULL, 0}
  };

  // Default bots
  params->maxBots = kMaxBots;

  // Setup default SSL config
  snprintf(params->caCertFilePath, kMaxPath - 1, "%s/%s", getenv("HOME"), kDefaultCACertFilePath);
  snprintf(params->caCertDirPath, kMaxPath - 1, "%s/%s", getenv("HOME"), kDefaultCACertDirPath);

  // Setup log directory
  snprintf(
    clientOptions.logDirPath, sizeof(clientOptions.logDirPath),
    "%s/.local/state/%s", getenv("HOME"), APPNAME
  );
  if (!TouchDir(clientOptions.logDirPath, 0700)) {
    fprintf(stderr, "Unable to set the log directory: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Parse the command line arguments into options
  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "n:h", options, NULL);
    if( (signed char)ch == -1 ) break; // No more options available
    switch (ch) {
      case 'h': // User requested help, display it and exit
        help(argv[0]);
        exit(EXIT_SUCCESS);
      break;
      case 'n': // User passed a number of desired bots
        params->maxBots = atoi(optarg);
      break;
      case 'f': // User passed a CA certificate file
        strncpy(params->caCertFilePath, optarg, kMaxPath - 1);
      break;
      case 'd': // User passed a CA directory path
        strncpy(params->caCertDirPath, optarg, kMaxPath - 1);
      break;
      case 0:
        if (debug) clientOptions.logLevel = LOG_DEBUG;
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

  // Copy SSL details into client options (prevent overflow by using he destination size)
  strncpy(clientOptions.caCertFilePath, params->caCertFilePath, sizeof(clientOptions.caCertFilePath));
  strncpy(clientOptions.caCertDirPath, params->caCertDirPath, sizeof(clientOptions.caCertDirPath));
}

/**
 * Displays program help
 */
void help(const char *program) {
  fprintf(stderr,
    "%1$s - commandline C2Hat Bot utility\n"
    "\n"
    "Usage: %1$s [options] <host> <port>\n"
    "       %1$s [-n HowManyBots] <host> <port>\n"
    "\n"
    "Current options include:\n"
    "   -n, --num-bots  specify how many bot threads to use;\n"
    "       --cacert    specify a CA certificate to verify with;\n"
    "       --capath    specify a directory where trusted CA certificates\n"
    "                   are stored; if neither cacert and capath are\n"
    "                   specified, the default path will be used:\n"
    "                   $HOME/.local/share/c2hat/ssl\n"
    "   -h, --help      display this help message;\n"
    "       --debug     enable verbose logging;\n"
    "\n", basename((char *)program)
  );
}

