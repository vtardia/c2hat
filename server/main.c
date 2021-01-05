#include "server.h"

#include <ctype.h>

const int kMaxClients = 5;
const int kServerPort = 10000;
const char *kServerHost = "localhost";

const int kMaxCommandSize = 10;
const char *kCommandStart = "start";
const char *kCommandStop = "stop";
const char *kCommandStatus = "status";

#ifdef __linux__
  const char *kPIDFile = "/var/run/demo.pid";
  const char *kLogFile = "/var/log/demo.log";
#else
  #ifdef __APPLE__
    const char *kPIDFile = "/usr/local/var/run/demo.pid";
    const char *kLogFile = "/usr/local/var/log/demo.log";
  #else
    const char *kPIDFile = "/tmp/demo.pid";
    const char *kLogFile = "/tmp/demo.log";
  #endif
#endif

// Close open resources and deletes PID file
void clean() {
  Info("Cleaning up...");
  remove(kPIDFile);
}

void parseCommand(char *dest, const char *arg) {
  strncpy(dest, arg, kMaxCommandSize -1);
  for (int i = 0; i < kMaxCommandSize; ++i) {
    dest[i] = tolower(dest[i]);
  }
  dest[kMaxCommandSize - 1] = '\0';
}

int CMD_runStart() {
  pid_t child = fork();
  if (child > 0) {
    // I'm in parent process
    fprintf(stdout, "Starting server process with PID %d\n", child);
    return EXIT_SUCCESS;
  }

  // I'm the child process now

  // Register shutdown function to close resource handlers
  atexit(clean);

  // Init PID file
  pid_t pid = PID_init(kPIDFile);

  // Init log facility
  if (LogInit(L_INFO, stderr, kLogFile) < 0) {
    fprintf(stderr, "Unable to initialise the logger: %s\n", strerror(errno));
    fprintf(stdout, "Unable to initialise the logger: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }
  Info("Starting PID %u...", pid);

  // Windows socket init
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  // Create a listening socket
  SOCKET server = Server_new(kServerHost, kServerPort, kMaxClients);

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
  PID_check(kPIDFile);
  pid_t pid = PID_load(kPIDFile);
  printf("The server is running with PID %d\n", pid);
  if (kill(pid, SIGTERM) == -1) {
    printf("Unable to kill process %d", pid);
    exit(EXIT_FAILURE);
  }
  printf("The server with PID %d has been successfully stopped\n", pid);
  return EXIT_SUCCESS;
}

// Check the status of the server daemon
int CMD_runStatus() {
  PID_check(kPIDFile);
  pid_t pid = PID_load(kPIDFile);
  printf("The server is running with PID %d, check '%s' for details\n", pid, kLogFile);
  return EXIT_SUCCESS;
}

void usage(const char *program) {
  fprintf(stderr, "Usage: %s [start|stop|status]\n", program);
}

int main(int argc, char const *argv[]) {
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  char command[10];
  parseCommand(command, argv[1]);

  if (strcmp(kCommandStart, command) == 0) return CMD_runStart();

  if (strcmp(kCommandStatus, command) == 0) return CMD_runStatus();

  if (strcmp(kCommandStop, command) == 0) return CMD_runStop();

  fprintf(stderr, "Unknown command: '%s'\n", command);
  usage(argv[0]);
  return EXIT_FAILURE;
}
