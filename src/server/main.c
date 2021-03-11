/*
 * Copyright (C) 2020 Vito Tardia
 */

/// @file main.c
#include "server.h"

#include <ctype.h>
#include <getopt.h>

/// Default connection limit
const int kDefaultMaxClients = 5;

/// Default server port
const int kDefaultServerPort = 10000;

/// Default server host
const char *kDefaultServerHost = "localhost";

/// Max length of a server command
const int kMaxCommandSize = 10;

/// ID string for the START command
const char *kCommandStart = "start";

/// ID string for the STOP command
const char *kCommandStop = "stop";

/// ID string for the STATUS command
const char *kCommandStatus = "status";

/// Shared memory handle that stores the configuration data
const char *kServerSharedMemPath = "/c2hat";

/// Size of the shared memory
const size_t kServerSharedMemSize = sizeof(ServerConfigInfo);


/**
 * PID File
 * On macOS and Linux it should be under /var/run or,
 * if not writable, the current user home directory.
 *
 * Not sure what to do in Windows
 *
 * Keeping it temporarily under /tmp
 */
#if defined(_WIN32)
  const char *kDefaultPIDFile = "C:\\Temp\\c2hat.pid";
#else
  const char *kDefaultPIDFile = "/tmp/c2hat.pid";
#endif

/**
 * Log File
 * On macOS can be under /var/log, /Library/Logs or ~/Library/Logs
 * On Linux can be under /var/log or ~/ (if not root)
 *
 * Not sure what to do in Windows
 *
 * Keeping it temporarily under /tmp
 */
#if defined(_WIN32)
  const char *kDefaultLogFile = "C:\\Temp\\c2hat.log";
#else
  const char *kDefaultLogFile = "/tmp/c2hat.log";
#endif

/// ARGV wrapper for options parsing
typedef char * const * ARGV;

/// Set to true when the server starts successfully
bool serverStartedSuccessfully = false;

void usage(const char *program);

/**
 * Closes any open resource and deletes PID file and shared memory
 */
void clean() {
  Info("Cleaning up...");
  // Remove PID file only if it was created by a successul server start
  if (serverStartedSuccessfully) {
    if (remove(kDefaultPIDFile) < 0) {
      Error("Unable to remove PID file: %s", strerror(errno));
    }
    if (!Config_clean(kServerSharedMemPath)) {
      Error("Unable to clean configuration: %s", strerror(errno));
    }
  }
}

/**
 * Parses the server command
 * Available commands are: start, stop, status
 * @param[out] dest Contains the parsed command
 * @param[in] arg The first argument from the ARGV array
 */
void parseCommand(char *dest, const char *arg) {
  strncpy(dest, arg, kMaxCommandSize -1);
  for (int i = 0; i < kMaxCommandSize; ++i) {
    dest[i] = tolower(dest[i]);
  }
  dest[kMaxCommandSize - 1] = '\0';
}

/**
 * Parses the command line options for the Start command
 * Returns true on success, false on failure
 * @param[in] argc The number of arguments
 * @param[in] argv The array of arguments
 * @param[out] host The listening IP address
 * @param[out] port The listening TCP port
 * @param[out] clients The maximum number of clients connected
 */
int parseOptions(int argc, ARGV argv, char **host, int *port, int *clients) {
  struct option options[] = {
    {"host", required_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"max-clients", required_argument, NULL, 'm'},
    { NULL, 0, NULL, 0}
  };

  int valid = true;

  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "h:p:m:", options, NULL);
    if( ch == -1 ) break; // no more options
    switch (ch) {
      case 'h': *host = optarg; break;
      case 'p': *port = atoi(optarg); break;
      case 'm': *clients = atoi(optarg); break;
      default:
        valid = false;
        break;
    }
  }
  return valid;
}

/**
 * Starts the server
 * @param[in] host Listening IP address
 * @param[in] port Listening TCP port
 * @param[in] maxClients Maximum number of client connections
 */
int CMD_runStart(const char *host, const int port, const int maxClients) {
  pid_t child = fork();
  if (child > 0) {
    // I'm in parent process
    fprintf(stdout, "Starting server on %s:%d with PID %d\n", host, port, child);
    return EXIT_SUCCESS;
  }

  // I'm the child process now

  // Register shutdown function to close resource handlers
  atexit(clean);

  // Init log facility
  if (LogInit(L_INFO, stderr, kDefaultLogFile) < 0) {
    fprintf(stderr, "Unable to initialise the logger (%s): %s\n", kDefaultLogFile, strerror(errno));
    fprintf(stdout, "Unable to initialise the logger (%s): %s\n", kDefaultLogFile, strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Windows socket init
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  // Initialise the chat server
  Server *server = Server_init(host, port, maxClients);

  // Init PID file (after server creation so we don't create on failure)
  pid_t pid = PID_init(kDefaultPIDFile);

  // The PID file can be safely deleted on exit
  serverStartedSuccessfully = true;

  // Write configuration to a shared memory location
  ServerConfigInfo currentConfig;
  memset(&currentConfig, 0, sizeof(ServerConfigInfo));
  currentConfig.pid = pid;
  memcpy(&(currentConfig.logFilePath), kDefaultLogFile, strlen(kDefaultLogFile));
  memcpy(&(currentConfig.pidFilePath), kDefaultPIDFile, strlen(kDefaultPIDFile));
  memcpy(&(currentConfig.host), host, strlen(host));
  currentConfig.port = 10000;
  currentConfig.maxConnections = maxClients;

  if (!Config_save(&currentConfig, sizeof(ServerConfigInfo), kServerSharedMemPath)) {
    return EXIT_FAILURE;
  }

  Info("Starting on %s:%d with PID %u and %d clients...", host, port, pid, maxClients);

  // Start the chat server (infinite loop until SIGTERM)
  Server_start(server);

  // Windows socket cleanup
#if defined(_WIN32)
  WSACleanup();
#endif

  return EXIT_SUCCESS;
}

/**
 * Stop the running server
 */
int CMD_runStop() {
  // Load configuration
  ServerConfigInfo *currentConfig = (ServerConfigInfo *)Config_load(kServerSharedMemPath, sizeof(ServerConfigInfo));
  if (currentConfig == NULL) {
    if (errno == ENOENT) {
      fprintf(stderr, "The server may not be running\n");
    } else {
      fprintf(stderr, "Unable to load configuration from shared memory: %s\n", strerror(errno));
    }
    return EXIT_FAILURE;
  }

  int pidStatus = PID_exists(currentConfig->pid);
  int result;

  // Existing PID
  if (pidStatus > 0) {
    printf("The server is running with PID %d\n", currentConfig->pid);
    if (kill(currentConfig->pid, SIGTERM) == -1) {
      printf("Unable to kill process %d: %s\n", currentConfig->pid, strerror(errno));
      result = EXIT_FAILURE;
    } else {
      printf("The server with PID %d has been successfully stopped\n", currentConfig->pid);
      // The server daemon will take care of cleaning the PID file and shared memory
      result = EXIT_SUCCESS;
    }
  }

  // Non-existing PID
  if (pidStatus == 0) {
    printf("Unable to check for PID %d: the server may not be running\n", currentConfig->pid);
    // Enable cleaning the shared memory and PID file
    serverStartedSuccessfully = true;
    result = EXIT_FAILURE;
  }

  // Error checking PID
  // We don't delete the PID file or shared memory because it may be
  // that the current user has not access to the PID
  if (pidStatus < 0 ) {
    printf("Error while checking for PID %d: %s\n", currentConfig->pid, strerror(errno));
    result = EXIT_FAILURE;
  }

  // Cleanup configuration data
  memset(currentConfig, 0, sizeof(ServerConfigInfo));
  free(currentConfig);
  currentConfig = NULL;

  return result;
}

/**
 * Check the status of the server daemon
 */
int CMD_runStatus() {
  // Load configuration
  ServerConfigInfo *currentConfig = (ServerConfigInfo *)Config_load(kServerSharedMemPath, sizeof(ServerConfigInfo));
  if (currentConfig == NULL) {
    if (errno == ENOENT) {
      fprintf(stderr, "The server may not be running\n");
    } else {
      fprintf(stderr, "Unable to load configuration from shared memory: %s\n", strerror(errno));
    }
    return EXIT_FAILURE;
  }

  int pidStatus = PID_exists(currentConfig->pid);
  int result;
  switch (pidStatus) {
    // The process esists
    case 1:
      printf("\nThe server is running with the following configuration:\n");
      printf("         PID: %d\n", currentConfig->pid);
      printf("    Log file: %s\n", currentConfig->logFilePath);
      printf("    PID file: %s\n", currentConfig->pidFilePath);
      printf("        Host: %s\n", currentConfig->host);
      printf("        Port: %d\n", currentConfig->port);
      printf(" Max Clients: %d\n", currentConfig->maxConnections);
      printf("\n");
      result = EXIT_SUCCESS;
    break;

    // The process does not esists
    case 0:
      printf("Unable to check for PID %d: the server may not be running\n", currentConfig->pid);
      // Enable cleaning the shared memory and PID file
      serverStartedSuccessfully = true;
      result = EXIT_FAILURE;
    break;

    // Cannot access the process info
    default:
      printf("Error while checking for PID %d: %s\n", currentConfig->pid, strerror(errno));
      result = EXIT_FAILURE;
    break;
  }

  // Cleanup configuration data
  memset(currentConfig, 0, sizeof(ServerConfigInfo));
  free(currentConfig);
  currentConfig = NULL;

  return result;
}

/**
 * Displays program usage
 */
void usage(const char *program) {
  fprintf(stderr, "Usage: %s [start [-h <host>] [-p <port>] [-m <max-clients>]|stop|status]\n", program);
}

/**
 * Main server entry point
 */
int main(int argc, ARGV argv) {
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  char command[10] = {0};
  parseCommand(command, argv[1]);

  if (strcmp(kCommandStart, command) == 0 && argc <= 6) {
    int maxClients = kDefaultMaxClients;
    int serverPort = kDefaultServerPort;
    char *serverHost = (char*)kDefaultServerHost;
    if (parseOptions(argc, argv, &serverHost, &serverPort, &maxClients)) {
      return CMD_runStart(serverHost, serverPort, maxClients);
    }
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (strcmp(kCommandStatus, command) == 0 && argc == 2) return CMD_runStatus();

  if (strcmp(kCommandStop, command) == 0 && argc == 2) return CMD_runStop();

  fprintf(stderr, "Unknown command: '%s'\n", command);
  usage(argv[0]);
  return EXIT_FAILURE;
}
