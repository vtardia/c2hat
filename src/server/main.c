/*
 * Copyright (C) 2020 Vito Tardia
 */

/// @file main.c
#include "server.h"

#include <ctype.h>
#include <getopt.h>
#include <sys/stat.h>
#include <locale.h>

#ifndef LOCALE
#define LOCALE "en_GB.UTF-8"
#endif

/// Current version
static const char *kC2HatServerVersion = "1.0";

/// Default connection limit
const int kDefaultMaxClients = 5;

/// Default server port
const int kDefaultServerPort = 10000;

/// Default server host
#define kDefaultServerHost "localhost"

/// Max length of a server command
const int kMaxCommandSize = 10;

/// Shared memory handle that stores the configuration data
const char *kServerSharedMemPath = "/c2hat";

/// Size of the shared memory
const size_t kServerSharedMemSize = sizeof(ServerConfigInfo);

/// Command identifiers
typedef enum Command {
  kCommandUnknown = -1,
  kCommandStart = 0,
  kCommandStop = 1,
  kCommandStatus = 2
} Command;

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
  // Remove PID file only if it was created by a successful server start
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
 * @param[in] argc The number of arguments for validation
 * @param[in] arg  The first argument from the ARGV array
 * @param[out]     The parsed command value
 */
Command parseCommand(int argc, const char *arg) {
  int result = kCommandUnknown;
  char *command = strndup(arg, kMaxCommandSize);
  for (char *c = command; *c != '\0'; c++) {
    *c = tolower(*c);
  }
  if (strcmp("start", command) == 0 && argc <= 9) {
    result = kCommandStart;
  }
  if (strcmp("stop", command) == 0 && argc == 2) {
    result = kCommandStop;
  }
  if (strcmp("status", command) == 0 && argc == 2) {
    result = kCommandStatus;
  }
  free(command);
  return result;
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
    {"ssl-cert", required_argument, NULL, 's'}, // leaving c for config
    {"ssl-key", required_argument, NULL, 'k'},
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
        memset(currentConfig->host, 0, strlen(currentConfig->host));
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
      case 's':
        memcpy(currentConfig->sslCertFilePath, optarg, strlen(optarg));
      break;
      case 'k':
        memcpy(currentConfig->sslKeyFilePath, optarg, strlen(optarg));
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
 * to clean the leftovers
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
  // Check locale compatibility
  if (strstr(LOCALE, "UTF-8") == NULL) {
    fprintf(stderr, "The given locale (%s) does not support UTF-8\n", LOCALE);
    return EXIT_FAILURE;
  }
  if (!setlocale(LC_ALL, LOCALE)) {
    fprintf(stderr, "Unable to set locale to '%s'\n", LOCALE);
    return EXIT_FAILURE;
  }

  if (strlen(currentConfig->sslCertFilePath) == 0) {
    fprintf(stderr, "SSL certificate file path missing: use --ssl-cert=/path/to/cert.pem\n");
    return EXIT_FAILURE;
  }
  if (strlen(currentConfig->sslKeyFilePath) == 0) {
    fprintf(stderr, "SSL private key file path missing: use --ssl-key=/path/to/key.pem\n");
    return EXIT_FAILURE;
  }

  if (runServerInForeground) {
    pid_t serverPID = getpid();
    fprintf(stdout, "Starting foreground server on %s:%d with locale '%s' and PID %d\n",
      currentConfig->host, currentConfig->port, LOCALE, serverPID
    );
  } else {
    pid_t sessionID;
    pid_t serverPID = fork();
    if (serverPID > 0) {
      // First fork, close the parent
      return EXIT_SUCCESS;
    }
    // serverPID = 0 means I'm in the child
    if (serverPID < 0) {
      fprintf(stderr, "Unable to start daemon server(1): %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    // I'm in the child, form again
    serverPID = fork();
    if (serverPID < 0) {
      fprintf(stderr, "Unable to start daemon server(2): %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    if (serverPID > 0) {
      // I'm in second parent process
      fprintf(stdout, "Starting background server on %s:%d with locale '%s' and PID %d\n",
        currentConfig->host, currentConfig->port, LOCALE, serverPID
      );
      return EXIT_SUCCESS;
    }
    // I'm in the final child
    umask(0);
    chdir("/usr/local");
    sessionID = setsid();
    if (sessionID < 0) {
      fprintf(stderr, "Unable to set new session: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
  }

  // If foreground is enabled I'm the main process, otherwise I'm the child

  // Register shutdown function to close resource handlers
  atexit(clean);

  // Init log facility and close unused streams
  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  currentLogFilePath = GetLogFilePath(); // Can take arguments in the future, like use stdin/out
  if (!vLogInit(LOG_INFO, currentLogFilePath)) {
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
    currentConfig->host, currentConfig->port, currentConfig->maxConnections,
    currentConfig->sslCertFilePath, currentConfig->sslKeyFilePath
  );

  // Init PID file (after server creation, so we don't create on failure)
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
  if (currentLogFilePath != NULL) {
    memcpy(currentConfig->logFilePath, currentLogFilePath, strlen(currentLogFilePath));
    free(currentLogFilePath);
  }
  memcpy(currentConfig->pidFilePath, currentPIDFilePath, strlen(currentPIDFilePath));

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
  // We don't delete the PID file or shared memory because
  // the current user may not have access permissions the PID file
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
    // The process exists
    case 1:
      printf("\nThe server is running with the following configuration:\n");
      printf("         PID: %d\n", currentConfig->pid);
      printf("    Log file: %s\n", currentConfig->logFilePath);
      printf("    PID file: %s\n", currentConfig->pidFilePath);
      printf("        Host: %s\n", currentConfig->host);
      printf("        Port: %d\n", currentConfig->port);
      printf("    SSL cert: %s\n", currentConfig->sslCertFilePath);
      printf("     SSL key: %s\n", currentConfig->sslKeyFilePath);
      printf("      Locale: %s\n", currentConfig->locale);
      printf(" Max Clients: %d\n", currentConfig->maxConnections);
      printf("\n");
      result = EXIT_SUCCESS;
    break;

    // The process does not exist
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
  fprintf(stderr,
"%1$s - Free TCP Chat Server [version %2$s]\n"
"\n"
"Usage: %1$s <command> [options]\n"
"       %1$s start --ssl-cert=</path/to/cert.pem> --ssk-key=</path/to/key.pem>\n"
"                    [--foreground] [-h <host>] [-p <port>] [-m <max-clients>]\n"
"       %1$s stop\n"
"       %1$s status\n"
"\n"
"Current available commands are:\n"
"       start          start the chat server;\n"
"       stop           stop the chat server, if running in background;\n"
"       status         display the chat server status and configuration;\n"
"\n"
"Current start options include:\n"
"       --ssl-cert     specify the path for the server TLS certificate;\n"
"       --ssl-key      specify the path for the server private key\n"
"   -h, --host         specify the listening host name or IP (default = localhost);\n"
"   -p, --host         specify the listening TCP port number (default = 10000);\n"
"   -m, --max-clients  specify the maximum number of connections (default = 5);\n"
"       --foreground   run the server in foreground;\n"
"\n", basename((char *)program), kC2HatServerVersion);
}

/**
 * Main server entry point
 */
int main(int argc, ARGV argv) {
  // At least a command is required
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  Command command = parseCommand(argc, argv[1]);

  if (kCommandStart == command) {
    // Initialise a default configuration
    ServerConfigInfo currentConfig = {
      .host = kDefaultServerHost,
      .port = kDefaultServerPort,
      .maxConnections = kDefaultMaxClients,
      .locale = LOCALE
    };
    if (parseOptions(argc, argv, &currentConfig)) {
      return CMD_runStart(&currentConfig);
    }
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (kCommandStatus == command) return CMD_runStatus();

  if (kCommandStop == command) return CMD_runStop();

  fprintf(stderr, "Unknown command: '%s'\n", argv[1]);
  usage(argv[0]);
  return EXIT_FAILURE;
}
