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

/// Tries to connect a client to the given chat server
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

  // Wait for the OK signal from the server
  // We are using select() with a timeout here, because if the server
  // doesn't have available connection slots, the client will remain hung.
  // To avoid this, we allow some second for the server to reply before failing
  char buffer[kBufferSize] = {0};
  fd_set reads;
  FD_ZERO(&reads);
  FD_SET(this->server, &reads);
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0; // micro seconds
  while(1) {
    if (!SOCKET_isValid(this->server)) return false;
    int result = select(this->server + 1, &reads, 0, 0, &timeout);
    if (result < 0) {
      if (SOCKET_getErrorNumber() == EINTR) break; // Signal received before timeout
      fprintf(this->err, "Client select() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      fflush(this->err);
      return false;
    }
    if (result == 0) {
      fprintf(this->err, "Client timeout expired\n");
      fflush(this->err);
      return false;
    }

    if (FD_ISSET(this->server, &reads)) {
      int received = Client_receive(this, buffer, kBufferSize);
      if (received < 0) {
        SOCKET_close(this->server);
        return false;
      }
      if (Message_getType(buffer) != kMessageTypeOk) {
        fprintf(this->err, "The server refused the connection\n");
        fflush(this->err);
        SOCKET_close(this->server);
        return false;
      }
      char *messageContent = Message_getContent(buffer, kMessageTypeOk, received);
      if (strlen(messageContent) > 0) {
        fprintf(this->err, "[Server]: %s\n", messageContent);
        fflush(this->err);
      }
      Message_free(&messageContent);
      break;
    }
  }
  return true;
}

/// Creates a new connected network chat client
C2HatClient *Client_create() {
  C2HatClient *client = calloc(sizeof(C2HatClient), 1);
  if (client == NULL) {
    fprintf(stderr, "Unable to create network client (%d) %s\n", errno, strerror(errno));
    return NULL;
  }
  client->in = stdin;
  client->out = stdout;
  client->err = stderr;
  return client;
}

/// Returns the raw client socket
SOCKET Client_getSocket(const C2HatClient *this) {
  if (this != NULL) return this->server;
  return 0;
}

/// Destroy a client object
void Client_destroy(C2HatClient **this) {
  if (this != NULL) {
    // Check if we need to close the socket
    if (*this != NULL) {
      C2HatClient *client = *this;
      if (SOCKET_isValid(client->server)) SOCKET_close(client->server);
      client = NULL;
    }
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
      return -1;
    }
    if (bytesReceived < 0) {
      fprintf(this->err, "recv() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      return -1;
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
    int bytesSent = send(this->server, data, length - total, MSG_NOSIGNAL);
    if (bytesSent < 0) {
      fprintf(this->err, "send() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      return -1;
    }
    data += bytesSent; // points to the remaining data to be sent
    total += bytesSent;
  } while (total < length);
  return total;
}

/// Authenticates with the C2Hat server
bool Client_authenticate(C2HatClient *this, const char *username) {
  if (strlen(username) < 1) {
    fprintf(this->out, "Invalid nickname\n");
    fflush(this->out);
    SOCKET_close(this->server);
    return false;
  }
  // Wait for the AUTH signal from the server
  char buffer[kBufferSize] = {0};
  int received = Client_receive(this, buffer, kBufferSize);
  if (received < 0) {
    SOCKET_close(this->server);
    return false;
  }
  if (Message_getType(buffer) != kMessageTypeNick) {
    fprintf(this->out, "Unable to authenticate\n");
    fflush(this->out);
    SOCKET_close(this->server);
    return false;
  }

  // Send credentials
  char message[kBufferSize] = {0};
  Message_format(kMessageTypeNick, message, kBufferSize, "%s", username);
  int sent = Client_send(this, message, strlen(message) + 1);
  if (sent < 0) {
    fprintf(this->out, "Unable to authenticate\n");
    fflush(this->out);
    SOCKET_close(this->server);
    return false;
  }

  // Wait for OK/ERR
  memset(buffer, 0, kBufferSize);
  received = Client_receive(this, buffer, kBufferSize);
  if (received < 0) {
    SOCKET_close(this->server);
    return false;
  }

  if (Message_getType(buffer) != kMessageTypeOk) {
    fprintf(this->out, "Authentication failed\n");
    fflush(this->out);
    SOCKET_close(this->server);
    return false;
  }
  return true;
}
