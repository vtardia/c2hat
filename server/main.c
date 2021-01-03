#include "server.h"

const int kMaxClients = 5;
const int kServerPort = 10000;
const char* kServerHost = "localhost";

int main(/*int argc, char const *argv[]*/) {
  // Windows initialisation
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

  // Windows cleanup
  #if defined(_WIN32)
    WSACleanup();
  #endif

  return EXIT_SUCCESS;
}
