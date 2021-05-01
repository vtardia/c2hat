/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "client.h"

#if defined(_WIN32)
  #include <conio.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <signal.h>

#include "socket/socket.h"
#include "message/message.h"

enum {
  kBufferSize = 1024 ///< Max size of data that can be sent, including the NULL terminator
};

/// Contains information on the current client application
typedef struct _C2HatClient {
  SOCKET server;
  FILE *in;
  FILE *out;
  FILE *err;
} C2HatClient;

/// Manages the infinite loop condition
static bool terminate = false;

/// Sets the termination flag on SIGINT or SIGTERM
void Client_stop(int signal) {
  printf("Received signal %d\n", signal);
  terminate = true;
}

/// Catches interrupt signals
int Client_catch(int sig, void (*handler)(int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   return sigaction (sig, &action, NULL);
}

// Tries to connect a client to the network
bool Client_connect(C2HatClient *this, const char *host, const char *port) {

  // Validate host and port information
  struct addrinfo options;
  memset(&options, 0, sizeof(options));
  struct addrinfo *bindAddress;
  options.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, port, &options, &bindAddress)) {
    fprintf(this->err, "Invalid IP/port configuration: %s", gai_strerror(SOCKET_getErrorNumber()));
    return false;
  }

  // Get a string representation of the host and port
  char addressBuffer[100] = {0};
  char serviceBuffer[100] = {0};
  if (getnameinfo(
    bindAddress->ai_addr, bindAddress->ai_addrlen,
    addressBuffer, sizeof(addressBuffer),
    serviceBuffer, sizeof(serviceBuffer),
    NI_NUMERICHOST
  )) {
    fprintf(this->err, "getnameinfo() failed (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
    return false;
  }

  // Try to create the socket
  this->server = socket(
    bindAddress->ai_family, bindAddress->ai_socktype, bindAddress->ai_protocol
  );
  if (!SOCKET_isValid(this->server)) {
    fprintf(this->err, "socket() failed (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
    return false;
  }

  // Try to connect
  fprintf(this->out, "Connecting to %s:%s...", addressBuffer, serviceBuffer);
  if (connect(this->server, bindAddress->ai_addr, bindAddress->ai_addrlen)) {
    fprintf(this->err, "connect() failed (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
    return false;
  }
  freeaddrinfo(bindAddress); // We don't need it anymore
  fprintf(this->out, "Connected!\n");
  return true;
}

/// Creates a new connected network chat client
C2HatClient *Client_create(const char *host, const char *port) {
  C2HatClient *client = calloc(sizeof(C2HatClient), 1);
  if (client == NULL) {
    fprintf(stderr, "Unable to create network client (%d) %s\n", errno, strerror(errno));
    return NULL;
  }
  client->in = stdin;
  client->out = stdout;
  client->err = stderr;
  if (!Client_connect(client, host, port)) {
    Client_destroy(&client);
    return NULL;
  }
  return client;
}

/// Destroy a client object
void Client_destroy(C2HatClient **this) {
  if (this != NULL) {
    memset(*this, 0, sizeof(C2HatClient));
    free(*this);
    *this = NULL;
  }
}

/// Receives data through the client's socket
int Client_receive(const C2HatClient *this, char *buffer, size_t length) {
  char *data = buffer; // points at the start of buffer
  size_t total = 0;
  char cursor[1] = {0};
  do {
    int bytesReceived = recv(this->server, cursor, 1, 0);
    if (bytesReceived == 0) {
      fprintf(this->out, "Connection closed by remote server\n");
      break; // exit the whole loop
    }
    if (bytesReceived < 0) {
      fprintf(this->err, "recv() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
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

/// Sends data through the client's socket
int Client_send(const C2HatClient *this, const char *buffer, size_t length) {
  // Ignore socket closed on send, will be caught by recv()
  size_t total = 0;
  char *data = (char*)buffer; // points to the beginning of the message
  do {
    int bytesSent = send(this->server, data, length - total, 0);
    if (bytesSent < 0) {
      fprintf(this->err, "send() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      break;
    }
    data += bytesSent; // points to the remaining data to be sent
    total += bytesSent;
  } while (total < length);
  return total;
}

/// Runs the client's infinite loop
void Client_run(C2HatClient *this, FILE *in, FILE *out, FILE *err) {
  Client_catch(SIGINT, Client_stop);
  Client_catch(SIGTERM, Client_stop);

  // Override default streams
  if (in == NULL) this->in = in;
  if (out == NULL) this->out = out;
  if (err == NULL) this->err = err;

  char buffer[kBufferSize] = {0};
  bool authenticated = false;

  // Wait for the OK signal from the server
  int received = Client_receive(this, buffer, kBufferSize);
  if (received > 0) {
    if (Message_getType(buffer) != kMessageTypeOk) {
      fprintf(this->out, "The server refused the connection\n");
      terminate = true;
    }
    char *messageContent = Message_getContent(buffer, kMessageTypeOk, received);
    if (strlen(messageContent) > 0) {
      fprintf(this->err, "[Server]: %s\n", messageContent);
    }
    Message_free(&messageContent);
    fprintf(this->out, " => To send data, enter text followed by enter.\n");
  } else {
    terminate = true;
  }

  while(!terminate) {
    // Initialise the socket set
    fd_set reads;
    FD_ZERO(&reads);
    // Add our listening socket
    FD_SET(this->server, &reads);
  #if !defined(_WIN32)
    // On non-windows systems we add STDIN to the list
    // of monitored sockets
    FD_SET(fileno(this->in), &reads);
  #endif

    // The timeout is needed for Windows
    // If no socket activity happens within 100ms,
    // we check the terminal input manually
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // micro seconds
    if (select(this->server + 1, &reads, 0, 0, &timeout) < 0) {
      if (SOCKET_getErrorNumber() == EINTR) break; // Signal received before timeout
      fprintf(this->err, "select() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      return;
    }

    if (FD_ISSET(this->server, &reads)) {
      // We have data in a socket
      memset(buffer, 0, kBufferSize);
      received = Client_receive(this, buffer, kBufferSize);
      if (received <= 0) {
        terminate = true;
      }
      char *messageContent = NULL;
      switch (Message_getType(buffer)) {
        case kMessageTypeErr:
          messageContent = Message_getContent(buffer, kMessageTypeErr, received);
          fprintf(this->err, "[Error]: %s\n", messageContent);
        break;
        case kMessageTypeOk:
          messageContent = Message_getContent(buffer, kMessageTypeOk, received);
          if (strlen(messageContent) > 0) {
            fprintf(this->err, "[Server]: %s\n", messageContent);
          }
        break;
        case kMessageTypeLog:
          messageContent = Message_getContent(buffer, kMessageTypeLog, received);
          fprintf(this->err, "[Server]: %s\n", messageContent);
        break;
        case kMessageTypeMsg:
          messageContent = Message_getContent(buffer, kMessageTypeMsg, received);
          fprintf(this->err, "%s\n", messageContent);
        break;
        case kMessageTypeNick:
          messageContent = Message_getContent(buffer, kMessageTypeNick, received);
          fprintf(this->err, "[Server]: %s\n", messageContent);
        break;
        case kMessageTypeQuit:
          break;
        break;
        default:
          // Print up to byte_received from the server
          fprintf(this->err, "Received (%d bytes): %.*s\n", received, received, buffer);
        break;
      }
      Message_free(&messageContent);
    }

    // Check for terminal input
  #if defined(_WIN32)
    if(_kbhit()) {
  #else
    if(FD_ISSET(fileno(this->in), &reads)) {
  #endif
      memset(buffer, 0, kBufferSize);
      // fgets() always includes a newline...
      if (!fgets(buffer, kBufferSize, this->in)) break;
      // ...so we are replacing it with a null terminator
      char *end = buffer + strlen(buffer) -1;
      *end = 0;
      char message[kBufferSize] = {0};

      // If the input is not a command, wrap it into a message type
      if (!Message_getType(buffer)) {
        if (authenticated) {
          Message_format(kMessageTypeMsg, message, kBufferSize, "%s", buffer);
        } else {
          Message_format(kMessageTypeNick, message, kBufferSize, "%s", buffer);
          authenticated = true;
        }
      } else {
        // Send the message as is
        memcpy(message, buffer, kBufferSize);
      }

      fprintf(this->err, "Sending: %s\n", message);
      int sent = Client_send(this, message, strlen(message) + 1);
      if (sent > 0) {
        fprintf(this->err, "Sent %d bytes.\n", sent);
      }
    }
  }
  fprintf(this->err, "Closing connection...");
  SOCKET_close(this->server);
}
