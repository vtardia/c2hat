#include "client.h"

#if defined(_WIN32)
  #include <conio.h>
#endif

#include <string.h>
#include <stdbool.h>
#include <signal.h>

static bool terminate = false;

void Client_stop(int signal) {
  printf("Received signal %d\n", signal);
  terminate = true;
}

// Catch interrupt signals
int Client_catch(int sig, void (*handler)(int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   return sigaction (sig, &action, NULL);
}

SOCKET Client_connect(const char *host, const char *port) {

  struct addrinfo options;
  memset(&options, 0, sizeof(options));
  struct addrinfo *bindAddress;
  options.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, port, &options, &bindAddress)) {
    fprintf(stderr, "Invalid IP/port configuration: %s", gai_strerror(SOCKET_getErrorNumber()));
    return -1;
  }

  char addressBuffer[100] = {0};
  char serviceBuffer[100] = {0};
  if (getnameinfo(
    bindAddress->ai_addr, bindAddress->ai_addrlen,
    addressBuffer, sizeof(addressBuffer),
    serviceBuffer, sizeof(serviceBuffer),
    NI_NUMERICHOST
  )) {
    fprintf(stderr, "getnameinfo() failed (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
    return -1;
  }

  SOCKET server = socket(
    bindAddress->ai_family, bindAddress->ai_socktype, bindAddress->ai_protocol
  );
  if (!SOCKET_isValid(server)) {
    fprintf(stderr, "socket() failed (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
    return -1;
  }

  printf("Connecting to %s:%s...", addressBuffer, serviceBuffer);
  if (connect(server, bindAddress->ai_addr, bindAddress->ai_addrlen)) {
    fprintf(stderr, "connect() failed (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
    return -1;
  }
  freeaddrinfo(bindAddress); // We don't need it anymore
  printf("Connected!\n");

  Client_catch(SIGINT, Client_stop);
  Client_catch(SIGTERM, Client_stop);

  return server;
}

void Client_listen(SOCKET server) {
  while(!terminate) {
    // Initialise the socket set
    fd_set reads;
    FD_ZERO(&reads);
    // Add our listening socket
    FD_SET(server, &reads);
  #if !defined(_WIN32)
    // On non-windows system we add STDIN to the list
    // of monitored sockets
    FD_SET(fileno(stdin), &reads);
  #endif

    // The timeout is needed for Windows
    // If no socket activity happens within 100ms,
    // we check the terminal input manually
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // micro seconds
    if (select(server + 1, &reads, 0, 0, &timeout) < 0) {
      if (SOCKET_getErrorNumber() == EINTR) break; // Signal received before timeout
      fprintf(stderr, "select() failed. (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
      return;
    }

    if (FD_ISSET(server, &reads)) {
      // We have data in a socket
      char read[4096];
      int bytes_received = recv(server, read, 4096, 0);
      if (bytes_received == 0) {
        printf("Connection closed by remote server\n");
        break; // exit the whole loop
      }
      if (bytes_received < 0) {
        fprintf(stderr, "recv() failed. (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
        break; // exit the whole loop
      }
      // Print up to byte_received from the server
      printf("Received (%d bytes): %.*s", bytes_received, bytes_received, read);
    }

    // Check for terminal input
  #if defined(_WIN32)
    if(_kbhit()) {
  #else
    if(FD_ISSET(fileno(stdin), &reads)) {
  #endif
      char read[4096];
      // fgets() always includes a newline
      if (!fgets(read, 4096, stdin)) break;
      printf("Sending: %s", read);
      // Ignore socket closed on send, will be caught by recv()
      int bytes_sent = send(server, read, strlen(read), 0);
      if (bytes_sent < 0) {
        fprintf(stderr, "send() failed. (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
        continue;
      }
      printf("Sent %d bytes.\n", bytes_sent);
    }
  }
  printf("Closing connection...");
  SOCKET_close(server);
}
