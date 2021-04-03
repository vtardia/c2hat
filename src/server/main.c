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

const char *kDefaultLogFileName = "c2hat.log";
const char *kDefaultPIDFileName = "c2hat.pid";

char *currentLogFilePath = NULL;
char *currentPIDFilePath = NULL;

/// ARGV wrapper for options parsing
typedef char * const * ARGV;

/// Set to true when the server starts successfully
bool serverStartedSuccessfully = false;

bool runServerInForeground = false;

void usage(const char *program);

/**
 * Closes any open resource and deletes PID file and shared memory
 */
void clean() {
  Info("Cleaning up...");
  // Remove PID file only if it was created by a successul server start
  if (serverStartedSuccessfully) {
    if (currentPIDFilePath != NULL) {
      if (remove(currentPIDFilePath) < 0) {
        Error("Unable to remove PID file: %s", strerror(errno));
      }
      free(currentPIDFilePath);
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
 * @param[in] currentConfig Pointer to a configuration structure
 */
int parseOptions(int argc, ARGV argv, ServerConfigInfo *currentConfig) {
  struct option options[] = {
    {"host", required_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"max-clients", required_argument, NULL, 'm'},
    {"foreground", no_argument, NULL, 'f'},
    { NULL, 0, NULL, 0}
  };

  int valid = true;

  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "h:p:m:", options, NULL);
    if( (signed char)ch == -1 ) break; // no more options
    switch (ch) {
      case 'h':
        memcpy(currentConfig->host, optarg, strlen(optarg));
      break;
      case 'p':
        currentConfig->port = atoi(optarg);
      break;
      case 'm':
        currentConfig->maxConnections = atoi(optarg);
      break;
      case 'f':
        runServerInForeground = true;
      break;
      default:
        valid = false;
        break;
    }
  }
  return valid;
}

/**
 * Returns a usable and accessible file path or NULL for stderr
 */
char *GetLogFilePath() {
  if (runServerInForeground) return NULL;
#if defined(WIN32)
  return strdup(kDefaultLogFile);
#else
  const char *logFileName = kDefaultLogFileName;
  char home[255] = {0};
  strcat(home, getenv("HOME"));
  #if defined(LINUX)
    char *paths[] = {
      "/var/log",
      home,
      "/tmp"
    };
    int pathsc = 3;
  #else
    #if defined(MACOS)
    char *paths[] = {
      "/var/log",
      "/Library/Logs",
      strcat(home, "/Library/Logs"),
      home,
      "/tmp"
    };
    int pathsc = 5;
    #else
    char *paths[] = {
      "/tmp"
    };
    int pathsc = 1;
    #endif
  #endif
  for (int i = 0; i < pathsc; ++i) {
    if (access(paths[i], R_OK | W_OK) == 0) {
      char *result = (char *)calloc(strlen(paths[i]) + strlen(logFileName) + 2, 1);
      if (!result) return NULL;
      return strcat(strcat(strcat(result, paths[i]), "/"), logFileName);
    }
  }
  return NULL;
#endif
}

/**
 * Returns a usable and accessible file path or NULL for stderr
 */
char *GetPIDFilePath() {
#if defined(WIN32)
  return strdup(kDefaultPIDFile);
#else
  const char *pidFileName = kDefaultPIDFileName;
  #if defined(LINUX)
    char *paths[] = {
      "/var/run",
      getenv("HOME"),
      "/tmp"
    };
    int pathsc = 3;
  #else
    #if defined(MACOS)
    char *paths[] = {
      "/var/run",
      getenv("HOME"),
      "/tmp"
    };
    int pathsc = 3;
    #else
    char *paths[] = {
      "/tmp"
    };
    int pathsc = 1;
    #endif
  #endif
  for (int i = 0; i < pathsc; ++i) {
    if (access(paths[i], R_OK | W_OK) == 0) {
      char *result = (char *)calloc(strlen(paths[i]) + strlen(pidFileName) + 2, 1);
      if (!result) return NULL;
      return strcat(strcat(strcat(result, paths[i]), "/"), pidFileName);
    }
  }
  return NULL;
#endif
}

/**
 * Clean facility called by STATUS and STOP commands
 * to clean the leftofers
 */
void cleanup() {
  if (currentPIDFilePath != NULL) {
    if (remove(currentPIDFilePath) < 0) {
      fprintf(stderr, "Unable to remove PID file: %s", strerror(errno));
    }
    free(currentPIDFilePath);
    currentPIDFilePath = NULL;
  }
  if (!Config_clean(kServerSharedMemPath)) {
    fprintf(stderr, "Unable to clean configuration: %s", strerror(errno));
  }
}


/**
 * Starts the server
 * @param[in] currentConfig Pointer to a configuration structure
 */
int CMD_runStart(ServerConfigInfo *currentConfig) {
  if (runServerInForeground) {
    pid_t serverPID = getpid();
    fprintf(stdout, "Starting foreground server on %s:%d with PID %d\n",
      currentConfig->host, currentConfig->port, serverPID
    );
  } else {
    pid_t serverPID = fork();
    if (serverPID > 0) {
      // I'm in parent process
      fprintf(stdout, "Starting background server on %s:%d with PID %d\n",
        currentConfig->host, currentConfig->port, serverPID
      );
      return EXIT_SUCCESS;
    }
    // serverPID = 0 means I'm in the child
    if (serverPID < 0) {
      fprintf(stderr, "Unable to start daemon server: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
  }

  // If foreground is enabled I'm the main process, otherwise I'm the child

  // Register shutdown function to close resource handlers
  atexit(clean);

  // Init log facility
  currentLogFilePath = GetLogFilePath(); // Can take arguments in the future, like use stdin/out
  if (LogInit(L_INFO, stderr, currentLogFilePath) < 0) {
    fprintf(stderr, "Unable to initialise the logger (%s): %s\n", currentLogFilePath, strerror(errno));
    fprintf(stdout, "Unable to initialise the logger (%s): %s\n", currentLogFilePath, strerror(errno));
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
  Server *server = Server_init(
    currentConfig->host, currentConfig->port, currentConfig->maxConnections
  );

  // Init PID file (after server creation so we don't create on failure)
  currentPIDFilePath = GetPIDFilePath();
  if (currentPIDFilePath == NULL) {
    Error("Unable to initialise PID file '%s'", currentPIDFilePath);
    return EXIT_FAILURE;
  }
  pid_t pid = PID_init(currentPIDFilePath);

  // The PID file can be safely deleted on exit
  serverStartedSuccessfully = true;

  // Update the configuration and write it to a shared memory location
  currentConfig->pid = pid;
  if (currentLogFilePath == NULL) {
    memcpy(currentConfig->logFilePath, "STDOUT", strlen("STDOUT"));
  } else {
    memcpy(currentConfig->logFilePath, currentLogFilePath, strlen(currentLogFilePath));
  }
  memcpy(currentConfig->pidFilePath, currentPIDFilePath, strlen(currentPIDFilePath));

  if (currentLogFilePath != NULL) {
    free(currentLogFilePath);
  }

  if (!Config_save(currentConfig, sizeof(ServerConfigInfo), kServerSharedMemPath)) {
    return EXIT_FAILURE;
  }

  Info(
    "Starting on %s:%d with PID %u and %d clients...",
    currentConfig->host, currentConfig->port,
    currentConfig->pid, currentConfig->maxConnections
  );

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

  currentPIDFilePath = strdup(currentConfig->pidFilePath);
  int pidStatus = PID_exists(currentConfig->pid);
  int result;

  // Existing PID
  if (pidStatus > 0) {
    printf("The server is running with PID %d\n", currentConfig->pid);
    if (kill(currentConfig->pid, SIGTERM) < 0) {
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
    cleanup();
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
  if (currentPIDFilePath != NULL) free(currentPIDFilePath);
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
  currentPIDFilePath = strdup(currentConfig->pidFilePath);
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
      cleanup();
      result = EXIT_FAILURE;
    break;

    // Cannot access the process info
    default:
      printf("Error while checking for PID %d: %s\n", currentConfig->pid, strerror(errno));
      result = EXIT_FAILURE;
    break;
  }

  // Cleanup configuration data
  if (currentPIDFilePath != NULL) free(currentPIDFilePath);
  memset(currentConfig, 0, sizeof(ServerConfigInfo));
  free(currentConfig);
  currentConfig = NULL;

  return result;
}

/**
 * Displays program usage
 */
void usage(const char *program) {
  fprintf(stderr, "Usage: %s [start [-h <host>] [-p <port>] [-m <max-clients>] [--foreground]|stop|status]\n", program);
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

  if (strcmp(kCommandStart, command) == 0 && argc <= 9) {
    // Initialise a default configuration
    ServerConfigInfo currentConfig;
    memset(&currentConfig, 0, sizeof(ServerConfigInfo));
    memcpy(&(currentConfig.host), kDefaultServerHost, strlen(kDefaultServerHost));
    currentConfig.port = kDefaultServerPort;
    currentConfig.maxConnections = kDefaultMaxClients;
    if (parseOptions(argc, argv, &currentConfig)) {
      return CMD_runStart(&currentConfig);
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
