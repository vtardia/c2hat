#include "server.h"

#include <ctype.h>
#include <getopt.h>

const int kDefaultMaxClients = 5;
const int kDefaultServerPort = 10000;
const char *kDefaultServerHost = "localhost";

const int kMaxCommandSize = 10;
const char *kCommandStart = "start";
const char *kCommandStop = "stop";
const char *kCommandStatus = "status";

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

typedef char * const * ARGV;

void usage(const char *program);

// Close open resources and deletes PID file
void clean() {
  Info("Cleaning up...");
  remove(kDefaultPIDFile);
}

void parseCommand(char *dest, const char *arg) {
  strncpy(dest, arg, kMaxCommandSize -1);
  for (int i = 0; i < kMaxCommandSize; ++i) {
    dest[i] = tolower(dest[i]);
  }
  dest[kMaxCommandSize - 1] = '\0';
}

int parseOptions(int argc, ARGV argv, char **host, int *port) {
  struct option options[] = {
    {"host", required_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    { NULL, 0, NULL, 0}
  };

  int valid = true;

  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "h:p:", options, NULL);
    if( ch == -1 ) break; // no more options
    switch (ch) {
      case 'h': *host = optarg; break;
      case 'p': *port = atoi(optarg); break;
      default:
        valid = false;
        break;
    }
  }
  return valid;
}

int CMD_runStart(const char *host, const int port) {
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

  // Create a listening socket
  SOCKET server = Server_new(host, port, kDefaultMaxClients);

  // Init PID file (after server creation so we don't create on failure)
  pid_t pid = PID_init(kDefaultPIDFile);
  Info("Starting on %s:%d with PID %u...", host, port, pid);

  // Start the chat server on that socket
  Server_start(server);

  // Windows socket cleanup
#if defined(_WIN32)
  WSACleanup();
#endif

  return EXIT_SUCCESS;
}

// Stop the running server
int CMD_runStop() {
  PID_check(kDefaultPIDFile);
  pid_t pid = PID_load(kDefaultPIDFile);
  printf("The server is running with PID %d\n", pid);
  if (kill(pid, SIGTERM) == -1) {
    printf("Unable to kill process %d\n", pid);
    exit(EXIT_FAILURE);
  }
  printf("The server with PID %d has been successfully stopped\n", pid);
  return EXIT_SUCCESS;
}

// Check the status of the server daemon
int CMD_runStatus() {
  PID_check(kDefaultPIDFile);
  pid_t pid = PID_load(kDefaultPIDFile);
  printf("The server is running with PID %d, check '%s' for details\n", pid, kDefaultLogFile);
  return EXIT_SUCCESS;
}

void usage(const char *program) {
  fprintf(stderr, "Usage: %s [start [-h <host>] [-p <port>]|stop|status]\n", program);
}

int main(int argc, ARGV argv) {
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  char command[10];
  parseCommand(command, argv[1]);

  if (strcmp(kCommandStart, command) == 0 && argc <= 6) {
    int serverPort = kDefaultServerPort;
    char *serverHost = (char*)kDefaultServerHost;
    if (parseOptions(argc, argv, &serverHost, &serverPort)) {
      return CMD_runStart(serverHost, serverPort);
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
