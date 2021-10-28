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
  SOCKET server; ///< Connection socket resource
  FILE *in;      ///< Input stream (currently unused)
  FILE *out;     ///< Output stream
  FILE *err;     ///< Error stream
} C2HatClient;

/**
 * Creates a new connected network chat client
 *
 * @param[out] A new C2HatClient instance or NULL on failure
 */
C2HatClient *Client_create() {
  C2HatClient *client = calloc(sizeof(C2HatClient), 1);
  if (client == NULL) {
    fprintf(stderr, "❌ Error: %d - Unable to create network client\n%s\n", errno, strerror(errno));
    return NULL;
  }
  client->in = stdin;
  client->out = stdout;
  client->err = stderr;
  return client;
}

/**
 * Tries to connect a client to the given chat server
 *
 * @param[in] this C2HatClient structure holding the connection information
 * @param[in] host Remote server host name or IP address
 * @param[in] post Remote server port
 * @param[out] Success or failure
 */
bool Client_connect(C2HatClient *this, const char *host, const char *port) {

  // Validate host and port information
  struct addrinfo options;
  memset(&options, 0, sizeof(options));
  struct addrinfo *bindAddress;
  options.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, port, &options, &bindAddress)) {
    fprintf(this->err, "❌ Error: Invalid IP/port configuration\n%s\n", gai_strerror(SOCKET_getErrorNumber()));
    return false;
  }

  // Get a string representation of the host and port
  char addressBuffer[100] = {0};
  char serviceBuffer[100] = {0};
  if (getnameinfo(
    bindAddress->ai_addr, bindAddress->ai_addrlen,
    addressBuffer, sizeof(addressBuffer),
    serviceBuffer, sizeof(serviceBuffer),
    NI_NUMERICHOST | NI_NUMERICSERV
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
  fprintf(this->err, "Connecting to %s:%s...", addressBuffer, serviceBuffer);
  if (connect(this->server, bindAddress->ai_addr, bindAddress->ai_addrlen)) {
    fprintf(this->err, "connect() failed (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
    return false;
  }
  freeaddrinfo(bindAddress); // We don't need it anymore
  fprintf(this->err, "Connected!\n");

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
      //fflush(this->err);
      return false;
    }
    if (result == 0) {
      fprintf(this->err, "Client timeout expired\n");
      //fflush(this->err);
      return false;
    }

    if (FD_ISSET(this->server, &reads)) {
      int received = Client_receive(this, buffer, kBufferSize);
      if (received < 0) {
        SOCKET_close(this->server);
        return false;
      }
      // At this point the server will only send /ok or /err messages
      if (Message_getType(buffer) != kMessageTypeOk) {
        fprintf(this->err, "❌ Error: Connection refused by the chat server\n");
        //fflush(this->err);
        SOCKET_close(this->server);
        return false;
      }
      // Display the server welcome message
      char *messageContent = Message_getContent(buffer, kMessageTypeOk, received);
      if (strlen(messageContent) > 0) {
        fprintf(this->err, "[Server] %s\n", messageContent);
        //fflush(this->err);
      }
      Message_free(&messageContent);
      break;
    }
  }
  return true;
}

/**
 * Returns the raw client socket
 *
 * This is handy when external application need to implement
 * a custom listen loop
 */
SOCKET Client_getSocket(const C2HatClient *this) {
  if (this != NULL) return this->server;
  return 0;
}

/**
 * Receives data from the server through the client's socket
 * until a null terminator is found or the buffer is full
 *
 * @param[in] this C2HatClient structure holding the connection information
 * @param[in] buffer Char buffer to store the received data
 * @param[in] length Length of the char buffer
 * @param[out] The number of bytes received
 */
int Client_receive(const C2HatClient *this, char *buffer, size_t length) {
  char *data = buffer; // points at the start of buffer
  size_t total = 0;
  char cursor[1] = {0};
  // Reading 1 byte at a time, until either the NULL terminator is found
  // or the given length is reached
  do {
    int bytesReceived = recv(this->server, cursor, 1, 0);
    if (bytesReceived == 0) {
      fprintf(this->err, "Connection closed by remote server\n");
      return -1;
    }
    if (bytesReceived < 0) {
      fprintf(this->err, "recv() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      // We don't care about partial read, it would be useless anyway
      return -1;
    }
    // Save data to the buffer and advance the cursor
    *data = cursor[0];
    data ++;
    total++;
    // Break the loop at length -1 so we can add the NULL terminator
    if (total == (length - 1)) break;
  } while(cursor[0] != 0);

  // Adding safe NULL terminator at the end
  if (*data != 0) *(data + 1) = 0;

  return total;
}

/**
 * Sends data through the client's socket using a loop
 * to ensure all the given data is sent
 *
 * @param[in] this C2HatClient structure holding the connection information
 * @param[in] buffer The bytes ot data to send
 * @param[in] length Size of the data to send
 * @param[out] Number of bytes sent
 */
int Client_send(const C2HatClient *this, const char *buffer, size_t length) {
  size_t total = 0;

  // Cursor pointing to the beginning of the message
  char *data = (char*)buffer;

  // Keep sending data until the buffer is empty
  do {
    int bytesSent = send(this->server, data, length - total, MSG_NOSIGNAL);
    if (bytesSent < 0) {
      fprintf(this->err, "send() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      return -1;
    }
    // Ignoring socket closed (byteSent == 0) on send, will be caught by recv()

    // Cursor now points to the remaining data to be sent
    data += bytesSent;
    total += bytesSent;
  } while (total < length);
  return total;
}

/**
 * Authenticates with the C2Hat server
 *
 * Currently the authentication consists only of a unique username
 * of max 20 chars. We are not validating the size from the client side
 * because the server will do this validation and trim the excess data
 *
 * @param[in] this C2HatClient structure holding the connection information
 * @param[in] username A NULL-terminated string containing the user's name
 * @param[out] Success or failure
 */
bool Client_authenticate(C2HatClient *this, const char *username) {
  // Minimal validation: ensure that at least two characters are entered
  if (strlen(username) < 2) {
    fprintf(this->err, "❌ Error: Invalid user nickname\nNicknames must be at least 2 characters long\n");
    //fflush(this->err);
    SOCKET_close(this->server);
    return false;
  }
  // Wait for the AUTH signal from the server,
  // we are ok for this to be blocking because
  // the server won't sent any data to unauthenticated clients
  char buffer[kBufferSize] = {0};
  int received = Client_receive(this, buffer, kBufferSize);
  if (received < 0) {
    SOCKET_close(this->server);
    return false;
  }
  if (Message_getType(buffer) != kMessageTypeNick) {
    fprintf(this->err, "❌ Error: Unable to authenticate\nUnknown server response\n");
    //fflush(this->err);
    SOCKET_close(this->server);
    return false;
  }

  // Send credentials
  char message[kBufferSize] = {0};
  Message_format(kMessageTypeNick, message, kBufferSize, "%s", username);
  int sent = Client_send(this, message, strlen(message) + 1);
  if (sent < 0) {
    fprintf(this->err, "❌ Error: Unable to authenticate\nCannot send data to the server, please retry later\n");
    //fflush(this->err);
    SOCKET_close(this->server);
    return false;
  }

  // Wait for OK/ERR
  memset(buffer, 0, kBufferSize);
  received = Client_receive(this, buffer, kBufferSize);
  if (received < 0) {
    fprintf(this->err, "❌ Error: Authentication failed\nCannot receive a response from the server\n");
    //fflush(this->err);
    SOCKET_close(this->server);
    return false;
  }

  int messageType = Message_getType(buffer);
  if (messageType != kMessageTypeOk) {
    if (messageType == kMessageTypeErr) {
      char *errorMessage = Message_getContent(buffer, kMessageTypeErr, kBufferSize);
      fprintf(this->err, "❌ [Server Error] Authentication failed\n%s\n", errorMessage);
      Message_free(&errorMessage);
    } else {
      fprintf(this->err, "❌ Error: Authentication failed\nInvalid response from the server\n");
    }
    //fflush(this->err);
    SOCKET_close(this->server);
    return false;
  }
  return true;
}

/**
 * Safely destroys a C2HatClient object
 */
void Client_destroy(C2HatClient **this) {
  if (this != NULL) {
    // Check if we need to close the socket
    if (*this != NULL) {
      C2HatClient *client = *this;
      if (SOCKET_isValid(client->server)) SOCKET_close(client->server);
      client = NULL;
    }
    // Erase the used memory
    memset(*this, 0, sizeof(C2HatClient));
    free(*this);
    *this = NULL;
  }
}
