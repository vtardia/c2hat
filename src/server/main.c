/*
 * Copyright (C) 2020 Vito Tardia
 */

/// @file main.c
#include "server.h"

#include <ctype.h>
#include <getopt.h>
#include <sys/stat.h>
#include <locale.h>
#include <libgen.h> // for basename() and dirname()

#include "ini/ini.h"
#include "logger/logger.h"

#define LOG_LEVEL_MATCH(value, level) (strncasecmp(value, level, 5) == 0)

#define LOG_LEVEL(level, value) { \
  if (LOG_LEVEL_MATCH(value, "trace")) { \
    level = LOG_TRACE; \
  } else if (LOG_LEVEL_MATCH(value, "debug")) { \
    level = LOG_DEBUG; \
  } else if (LOG_LEVEL_MATCH(value, "info")) { \
    level = LOG_INFO; \
  } else if (LOG_LEVEL_MATCH(value, "warn")) { \
    level = LOG_WARN; \
  } else if (LOG_LEVEL_MATCH(value, "error")) { \
    level = LOG_ERROR; \
  } else if (LOG_LEVEL_MATCH(value, "fatal")) { \
    level = LOG_FATAL; \
  } else { \
    level = LOG_DEFAULT; \
  } \
}

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
 * Gets the path of the configuration file to load
 * if no custom configuration is provided from the
 * command line.
 *
 * TODO Separate Win/Mac/Linux and wrap into a macro
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
char *GetConfigFilePath(char *filePath, size_t length) {
  char userConfigFilePath[kMaxPath] = {};
  snprintf(
    userConfigFilePath, sizeof(userConfigFilePath),
    "%s/.config/c2hat/c2hat-server.conf", getenv("HOME")
  );
  memset(filePath, 0, length);
  if (getuid() != 0 && access(userConfigFilePath, R_OK) == 0) {
    // User is not root and has a config file under their home dir
    memcpy(filePath, userConfigFilePath, length);
  } else if (access("/etc/c2hat/c2hat-server.conf", R_OK) == 0) {
    // User is root or there is no personal config file,
    // lookup global files
    memcpy(filePath, "/etc/c2hat/c2hat-server.conf", length);
  } else if (access("/usr/local/etc/c2hat/c2hat-server.conf", R_OK) == 0) {
    memcpy(filePath, "/usr/local/etc/c2hat/c2hat-server.conf", length);
  }
  // If none of the files exist an empty string is returned
  return filePath;
}

/**
 * Returns the path for the PID file
 *
 * TODO write Win/Mac/Linux versions and switch using a macro
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
char *GetDefaultPidFilePath(char *filePath, size_t length) {
  if (getuid() == 0) {
    // Running as root, use system directories
    snprintf(filePath, length, "/var/run/c2hat.pid");
  } else {
    // Running as local user, use local directory
    snprintf(filePath, length, "%s/.local/run/c2hat.pid", getenv("HOME"));
  }
  return filePath;
}

/**
 * Returns the path for the Log file
 *
 * TODO write Win/Mac/Linux versions and switch using a macro
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
char *GetDefaultLogFilePath(char *filePath, size_t length) {
  if (getuid() == 0) {
    // Running as root, use system path
    snprintf(filePath, length, "/var/log/c2hat-server.log");
  } else {
    // Running as local user, use local directory
    snprintf(filePath, length, "%s/.local/state/c2hat-server.log", getenv("HOME"));
  }
  return filePath;
}

void GetDefaultTlsFilePaths(
  char *certPath, size_t certLength,
  char *keyPath, size_t keyLength
) {
  // Paranoid reset of output buffers
  memset(certPath, 0, certLength);
  memset(keyPath, 0, keyLength);
  char userCertPath[kMaxPath] = {};
  char userKeyPath[kMaxPath] = {};
  snprintf(
    userCertPath, sizeof(userCertPath),
    "%s/.config/c2hat/ssl/cert.pem", getenv("HOME")
  );
  snprintf(
    userKeyPath, sizeof(userKeyPath),
    "%s/.config/c2hat/ssl/key.pem", getenv("HOME")
  );
  if (getuid() != 0 && access(userCertPath, R_OK) == 0
    && access(userKeyPath, R_OK) == 0) {
      // Not running as root and we have local files
      memcpy(certPath, userCertPath, certLength);
      memcpy(keyPath, userKeyPath, keyLength);
  } else if (access("/etc/c2hat/ssl/cert.pem", R_OK) == 0
    && access("/etc/c2hat/ssl/key.pem", R_OK) == 0) {
      // Running as root or no user files available
      // Try /etc/c2hat/ssl
      snprintf(certPath, certLength, "/etc/c2hat/ssl/cert.pem");
      snprintf(keyPath, keyLength, "/etc/c2hat/ssl/key.pem");
  } else if (access("/usr/local/etc/c2hat/ssl/cert.pem", R_OK) == 0
    && access("/usr/local/etc/c2hat/ssl/key.pem", R_OK) == 0) {
      // Try /usr/local/etc/c2hat/ssl
      snprintf(certPath, certLength, "/usr/local/etc/c2hat/ssl/cert.pem");
      snprintf(keyPath, keyLength, "/usr/local/etc/c2hat/ssl/key.pem");
  }
  // File paths will be empty at this point, will beed to be
  // overridden with a custom conf file or from the command line
}

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
      /* free(currentPIDFilePath); */
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
 * Handles INI key/value pairs
 *
 * For every tuple returns 1 on success, 0 on failure
 * @param[in] data    Pointer to a user data type to store results
 * @param[in] section Section for the current tuple, or empty string
 * @param[in] name    Name of the setting
 * @param[in] value   Value of the setting
 */
static int handler(
  void* data,
  const char* section,
  const char* name,
  const char* value
) {
  ServerConfigInfo* settings = data;
  #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
  if (MATCH("", "locale")) {
    memcpy(settings->locale, value, sizeof(settings->locale) -1);
  } else if (MATCH("log", "level")) {
    LOG_LEVEL(settings->logLevel, value);
  } else if (MATCH("log", "path")) {
    memcpy(settings->logFilePath, value, sizeof(settings->logFilePath) -1);
  } else if (MATCH("server", "host")) {
    memcpy(settings->host, value, sizeof(settings->host) -1);
  } else if (MATCH("server", "port")) {
    settings->port = atoi(value);
  } else if (MATCH("server", "max_connections")) {
    settings->maxConnections = atoi(value);
  } else if (MATCH("server", "pid_file_path")) {
    memcpy(settings->pidFilePath, value, sizeof(settings->pidFilePath) -1);
  } else if (MATCH("tls", "cert_file")) {
    memcpy(settings->sslCertFilePath, value, sizeof(settings->sslCertFilePath) -1);
  } else if (MATCH("tls", "key_file")) {
    memcpy(settings->sslKeyFilePath, value, sizeof(settings->sslKeyFilePath));
  } else {
    return 0; // unknown section/name or error
  }
  return 1; // success
}

/**
 * Parses the command line options for the Start command
 * Returns true on success, false on failure
 * @param[in] argc The number of arguments
 * @param[in] argv The array of arguments
 * @param[in] settings Pointer to a configuration structure
 */
int parseOptions(int argc, ARGV argv, ServerConfigInfo *settings) {
  // Fill some defaults first
  GetDefaultLogFilePath(settings->logFilePath, sizeof(settings->logFilePath));
  GetDefaultPidFilePath(settings->pidFilePath, sizeof(settings->pidFilePath));
  GetDefaultTlsFilePaths(
    settings->sslCertFilePath, sizeof(settings->sslCertFilePath),
    settings->sslKeyFilePath, sizeof(settings->sslKeyFilePath)
  );
  char configFilePath[kMaxPath] = {};
  char sslCertFilePath[kMaxPath] = {};
  char sslKeyFilePath[kMaxPath] = {};
  char host[kMaxHostLength] = {};
  unsigned int port = 0;
  unsigned int maxConnections = 0;

  // Collect and parse command line options
  struct option options[] = {
    {"config-file", required_argument, NULL, 'c'},
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
    if ((signed char)ch == -1) break; // no more options
    switch (ch) {
      case 'h':
        snprintf(host, sizeof(host) -1, "%s", optarg);
      break;
      case 'p':
        port = atoi(optarg);
      break;
      case 'm':
        maxConnections = atoi(optarg);
      break;
      case 'f':
        runServerInForeground = true;
      break;
      case 's':
        snprintf(sslCertFilePath, sizeof(sslCertFilePath) -1, "%s", optarg);
      break;
      case 'k':
        snprintf(sslKeyFilePath, sizeof(sslKeyFilePath) -1, "%s", optarg);
      break;
      case 'c':
        snprintf(configFilePath, sizeof(configFilePath) -1, "%s", optarg);
      break;
      default:
        valid = false;
        break;
    }
  }
  if (strlen(configFilePath) == 0) {
    // No config file has been specified, use default
    GetConfigFilePath(configFilePath, sizeof(configFilePath));
  }
  if (strlen(configFilePath) > 0) {
    // We have a config file to parse
    int result = ini_parse(configFilePath, handler, settings);
    valid = (result == 0);

    if (result == -1) {
      // file open error
      fprintf(stderr, "unable to open file '%s' - %s\n", configFilePath, strerror(errno));
      return false;
    }
    if (result == -2) {
      // memory allocation error
      fprintf(stderr, "memory allocation error - %s\n", strerror(errno));
      return false;
    }
    if (result > 0) {
      // parse error at line result
      fprintf(stderr, "parse error in %s at line %d\n", configFilePath, result);
      return false;
    }
  }
  // Now we can override with CLI options, if specified
  if (strlen(host) > 0) {
    memset(settings->host, 0, sizeof(settings->host));
    memcpy(settings->host, host, sizeof(settings->host) -1);
  }
  if (port > 0) {
    settings->port = port;
  }
  if (maxConnections > 0) {
    settings->maxConnections = maxConnections;
  }
  if (strlen(sslCertFilePath) > 0) {
    memset(settings->sslCertFilePath, 0, sizeof(settings->sslCertFilePath));
    memcpy(settings->sslCertFilePath, sslCertFilePath, sizeof(settings->sslCertFilePath) -1);
  }
  if (strlen(sslKeyFilePath) > 0) {
    memset(settings->sslKeyFilePath, 0, sizeof(settings->sslKeyFilePath));
    memcpy(settings->sslKeyFilePath, sslKeyFilePath, sizeof(settings->sslKeyFilePath) -1);
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
  char home[255] = {};
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
 * @param[in] settings Pointer to a configuration structure
 */
int CMD_runStart(ServerConfigInfo *settings) {
  // Calling setlocale() with an empty string loads the LANG env var
  if (!setlocale(LC_ALL, "")) {
    fprintf(stderr, "Unable to read locale");
    return EXIT_FAILURE;
  }

  // Calling setlocale() with a NULL argument reads the corresponding LC_ var
  // If the system does not support the locale it will return "C"
  // otherwise the full locale string (e.g. en_US.UTF-8)
  char *locale = setlocale(LC_ALL, NULL);
  memcpy(settings->locale, locale, sizeof(settings->locale) -1);

  // Check locale compatibility
  if (strstr(locale, "UTF-8") == NULL) {
    fprintf(stderr, "The given locale (%s) does not support UTF-8\n", locale);
    return EXIT_FAILURE;
  }

  if (strlen(settings->sslCertFilePath) == 0) {
    fprintf(stderr, "SSL certificate file path missing: use --ssl-cert=/path/to/cert.pem\n");
    return EXIT_FAILURE;
  }

  if (strlen(settings->sslKeyFilePath) == 0) {
    fprintf(stderr, "SSL private key file path missing: use --ssl-key=/path/to/key.pem\n");
    return EXIT_FAILURE;
  }

  if (runServerInForeground) {
    pid_t serverPID = getpid();
    fprintf(stdout, "Starting foreground server on %s:%d with locale '%s' and PID %d\n",
      settings->host, settings->port, settings->locale, serverPID
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
    // I'm in the child, fork again
    serverPID = fork();
    if (serverPID < 0) {
      fprintf(stderr, "Unable to start daemon server(2): %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    if (serverPID > 0) {
      // I'm in second parent process
      fprintf(stdout, "Starting background server on %s:%d with locale '%s' and PID %d\n",
        settings->host, settings->port, settings->locale, serverPID
      );
      return EXIT_SUCCESS;
    }
    // I'm in the final child
    umask(0);
    chdir("/usr/local"); // TODO: this should depend on the user's privilege
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
  if (runServerInForeground) {
    currentLogFilePath = NULL; // Log to stdout
  } else {
    // Try to create the log directory if not exists
    // TODO create a fileutil lib and call this TouchDir()
    currentLogFilePath = strdup(settings->logFilePath);
    char *path = strdup(settings->logFilePath);
    struct stat st = {};
    char *logDir = dirname(path);
    if (stat(logDir, &st) == -1) {
      if (mkdir(logDir, 0700) < 0) {
        if (path != NULL) free(path);
        Fatal("Unable to create log directory (%d)", logDir);
      }
    }
    if (path != NULL) free(path);
  }
  if (!vLogInit(LOG_INFO, currentLogFilePath)) {
    fprintf(stderr, "Unable to initialise the logger (%s): %s\n", currentLogFilePath, strerror(errno));
    fprintf(stdout, "Unable to initialise the logger (%s): %s\n", currentLogFilePath, strerror(errno));
    exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);

  // Windows socket init
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  // Initialise the chat server
  Server *server = Server_init(settings);

  // Init PID file (after server creation, so we don't create on failure)
  currentPIDFilePath = settings->pidFilePath;
  if (currentPIDFilePath == NULL) {
    Error("Unable to initialise PID file '%s'", currentPIDFilePath);
    return EXIT_FAILURE;
  }
  pid_t pid = PID_init(currentPIDFilePath);

  // The PID file can be safely deleted on exit
  serverStartedSuccessfully = true;

  // Update the configuration and write it to a shared memory location
  settings->pid = pid;
  if (currentLogFilePath != NULL) free(currentLogFilePath);

  if (!Config_save(settings, sizeof(ServerConfigInfo), kServerSharedMemPath)) {
    return EXIT_FAILURE;
  }

  Info(
    "Starting on %s:%d with PID %u and %d clients...",
    settings->host, settings->port,
    settings->pid, settings->maxConnections
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
  ServerConfigInfo *settings = (ServerConfigInfo *)Config_load(
    kServerSharedMemPath, sizeof(ServerConfigInfo)
  );
  if (settings == NULL) {
    if (errno == ENOENT) {
      fprintf(stderr, "The server may not be running\n");
    } else {
      fprintf(stderr, "Unable to load configuration from shared memory: %s\n", strerror(errno));
    }
    return EXIT_FAILURE;
  }

  currentPIDFilePath = strdup(settings->pidFilePath);
  int pidStatus = PID_exists(settings->pid);
  int result;

  // Existing PID
  if (pidStatus > 0) {
    printf("The server is running with PID %d\n", settings->pid);
    if (kill(settings->pid, SIGTERM) < 0) {
      printf("Unable to kill process %d: %s\n", settings->pid, strerror(errno));
      result = EXIT_FAILURE;
    } else {
      printf("The server with PID %d has been successfully stopped\n", settings->pid);
      // The server daemon will take care of cleaning the PID file and shared memory
      result = EXIT_SUCCESS;
    }
  }

  // Non-existing PID
  if (pidStatus == 0) {
    printf("Unable to check for PID %d: the server may not be running\n", settings->pid);
    // Enable cleaning the shared memory and PID file
    cleanup();
    result = EXIT_FAILURE;
  }

  // Error checking PID
  // We don't delete the PID file or shared memory because
  // the current user may not have access permissions the PID file
  if (pidStatus < 0 ) {
    printf("Error while checking for PID %d: %s\n", settings->pid, strerror(errno));
    result = EXIT_FAILURE;
  }

  // Cleanup configuration data
  if (currentPIDFilePath != NULL) free(currentPIDFilePath);
  memset(settings, 0, sizeof(ServerConfigInfo));
  free(settings);
  settings = NULL;

  return result;
}

/**
 * Check the status of the server daemon
 */
int CMD_runStatus() {
  // Load configuration
  ServerConfigInfo *settings = (ServerConfigInfo *)Config_load(
    kServerSharedMemPath, sizeof(ServerConfigInfo)
  );
  if (settings == NULL) {
    if (errno == ENOENT) {
      fprintf(stderr, "The server may not be running\n");
    } else {
      fprintf(stderr, "Unable to load configuration from shared memory: %s\n", strerror(errno));
    }
    return EXIT_FAILURE;
  }

  int pidStatus = PID_exists(settings->pid);
  currentPIDFilePath = strdup(settings->pidFilePath);
  int result;
  switch (pidStatus) {
    // The process exists
    case 1:
      printf("\nThe server is running with the following configuration:\n");
      printf("         PID: %d\n", settings->pid);
      printf("    Log file: %s\n", settings->logFilePath);
      printf("    PID file: %s\n", settings->pidFilePath);
      printf("        Host: %s\n", settings->host);
      printf("        Port: %d\n", settings->port);
      printf("    SSL cert: %s\n", settings->sslCertFilePath);
      printf("     SSL key: %s\n", settings->sslKeyFilePath);
      printf("      Locale: %s\n", settings->locale);
      printf(" Max Clients: %d\n", settings->maxConnections);
      printf("\n");
      result = EXIT_SUCCESS;
    break;

    // The process does not exist
    case 0:
      printf("Unable to check for PID %d: the server may not be running\n", settings->pid);
      // Enable cleaning the shared memory and PID file
      cleanup();
      result = EXIT_FAILURE;
    break;

    // Cannot access the process info
    default:
      printf("Error while checking for PID %d: %s\n", settings->pid, strerror(errno));
      result = EXIT_FAILURE;
    break;
  }

  // Cleanup configuration data
  if (currentPIDFilePath != NULL) free(currentPIDFilePath);
  // TODO: wrap into a macro and use memset_s/explicit_bzero
  memset(settings, 0, sizeof(ServerConfigInfo));
  free(settings);
  settings = NULL;

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
    ServerConfigInfo settings = {
      .host = kDefaultServerHost,
      .port = kDefaultServerPort,
      .maxConnections = kDefaultMaxClients,
      .logLevel = LOG_INFO
    };
    if (parseOptions(argc, argv, &settings)) {
      return CMD_runStart(&settings);
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
