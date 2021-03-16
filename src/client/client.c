/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "client.h"

#if defined(_WIN32)
  #include <conio.h>
#endif

#include <string.h>
#include <stdbool.h>
#include <signal.h>

enum {
  kBufferSize = 1024 // Includes NULL term
};

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

  return server;
}

int Client_receive(SOCKET server, char *buffer, size_t length) {
  char *data = buffer; // points at the start of buffer
  size_t total = 0;
  char cursor[1] = {0};
  do {
    int bytesReceived = recv(server, cursor, 1, 0);
    if (bytesReceived == 0) {
      printf("Connection closed by remote server\n");
      break; // exit the whole loop
    }
    if (bytesReceived < 0) {
      fprintf(stderr, "recv() failed. (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
      break; // exit the whole loop
    }
    *data = cursor[0];
    data ++;
    total++;
    if (total == (length - 1)) break;
  } while(cursor[0] != 0);

  // Adding safe terminator in case of loop break
  if (*data != 0) *(data + 1) = 0;

  return total;
}

int Client_send(SOCKET server, const char *buffer, size_t length) {
  // Ignore socket closed on send, will be caught by recv()
  size_t total = 0;
  char *data = (char*)buffer; // points to the beginning of the message
  do {
    int bytesSent = send(server, data, length - total, 0);
    if (bytesSent < 0) {
      fprintf(stderr, "send() failed. (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
      break;
    }
    data += bytesSent; // points to the remaining data to be sent
    total += bytesSent;
  } while (total < length);
  return total;
}

void Client_listen(SOCKET server) {
  Client_catch(SIGINT, Client_stop);
  Client_catch(SIGTERM, Client_stop);

  char read[kBufferSize] = {0};

  // Wait for the OK signal from the server
  int received = Client_receive(server, read, kBufferSize);
  if (received > 0) {
    if (strncmp(read, "/ok", 3) != 0) {
      printf("The server refused the connection\n");
      terminate = true;
    }
    printf("[server]: %s\n", (read + 4));
    printf("To send data, enter text followed by enter.\n");
  } else {
    terminate = true;
  }

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
      char read[kBufferSize] = {0};
      int received = Client_receive(server, read, kBufferSize);
      if (received <= 0) {
        terminate = true;
      }
      // Print up to byte_received from the server
      printf("Received (%d bytes): %.*s\n", received, received, read);
    }

    // Check for terminal input
  #if defined(_WIN32)
    if(_kbhit()) {
  #else
    if(FD_ISSET(fileno(stdin), &reads)) {
  #endif
      char read[kBufferSize] = {0};
      // fgets() always includes a newline...
      if (!fgets(read, kBufferSize, stdin)) break;
      // ...so we are replacing it with a null terminator
      char *end = read + strlen(read) -1;
      *end = 0;
      printf("Sending: %s\n", read);
      int sent = Client_send(server, read, (end - read));
      if (sent > 0) {
        printf("Sent %d bytes.\n", sent);
      }
    }
  }
  printf("Closing connection...");
  SOCKET_close(server);
}
