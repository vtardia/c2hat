/**
 * Copyright (C) 2020-2022 Vito Tardia <https://vito.tardia.me>
 *
 * This file is part of C2Hat
 *
 * C2Hat is a simple client/server TCP chat written in C
 *
 * C2Hat is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * C2Hat is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with C2Hat. If not, see <https://www.gnu.org/licenses/>.
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

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "socket/socket.h"
#include "message/message.h"
#include "logger/logger.h"

#define IsLocalhost(someAddress) ( \
  strcmp(someAddress, "127.0.0.1") == 0 || strcmp(someAddress, "::1") == 0 \
)

/// Contains information on the current client application
typedef struct _C2HatClient {
  SOCKET server;               ///< Connection socket resource
  FILE *in;                    ///< Input stream (currently unused)
  FILE *out;                   ///< Output stream
  FILE *err;                   ///< Error stream
  SSL_CTX *sslContext;         ///< SSL context resource
  SSL *ssl;                    ///< SSL connection handle
  STACK_OF(X509) *chain;       ///< SSL certificate chain from the server
  MessageBuffer buffer;        ///< Data read from server connection
  char logFilePath[kMaxPath];  ///< Path to the log file
  unsigned int logLevel;       ///< Default log level
} C2HatClient;

/// Keeps track of SSL initialisation that should happen only once
static bool sslInit = false;

/**
 * Initialises the OpenSSL library functions
 */
SSL_CTX *Client_ssl_init(const char *caCert, const char *caPath, char *error, size_t length) {
  // Initialise the OpenSSL library, must be done once
  if (!sslInit) {
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    sslInit = true;
  }

  // Create SSL context for the client
  SSL_CTX * context = SSL_CTX_new(TLS_client_method());
  if (!context) {
    strncpy(error, "SSL_CTX_new() failed.", length);
    return NULL;
  }

  // Set context options: minimum protocol version
  // Note: options set for the context are inherited by all SSL
  // connections created with this context and can be overridden
  // for each connection if needed
  if (!SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION)) {
    strncpy(error, "Cannot set minimum TLS protocol version.", length);
    return NULL;
  }

  // Set context options: use all possible bug fixes
  SSL_CTX_set_options(context, SSL_OP_ALL | SSL_OP_NO_RENEGOTIATION);

  // Set context operating mode
  SSL_CTX_set_mode(
    context,
    SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER
  );

  // Use the most up to date cipher list
  SSL_CTX_set_cipher_list(
    context,
    "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256"
  );

  // Check that the CA file and dir path exist
  char *caCertFile = NULL;
  char *caCertDir = NULL;
  if(access(caCert, R_OK ) != -1) {
    caCertFile = (char*)caCert;
  }
  if(access(caPath, R_OK ) != -1) {
    caCertDir = (char*)caPath;
  }

  // Load CA certificates locations
  if (!SSL_CTX_load_verify_locations(context, caCertFile, caCertDir)) {
    strncpy(error, "Unable to load CA locations", length);
    return NULL;
  }
  // Tell OpenSSL to automatically check the server certificate or fail
  SSL_CTX_set_verify(context, SSL_VERIFY_PEER, NULL);
  return context;
}

/**
 * Creates a new connected network chat client
 *
 * @param[in]  options Client startup configuration
 * @param[out] A new C2HatClient instance or NULL on failure
 */
C2HatClient *Client_create(ClientOptions *options) {
  C2HatClient *client = calloc(sizeof(C2HatClient), 1);
  if (client == NULL) {
    fprintf(
      stderr, "❌ Error: %d - Unable to create network client\n%s\n",
      errno, strerror(errno)
    );
    return NULL;
  }
  client->in = stdin;
  client->out = stdout;
  client->err = stderr;
  client->logLevel = options->logLevel;
  // Create the log file path, doing it in 2 steps because snprintf() complains
  // about a possible format overflow
  strncpy(client->logFilePath, options->logDirPath, sizeof(client->logFilePath));
  strncat(client->logFilePath, "/client.log", sizeof(client->logFilePath));

  char error[100] = {};
  client->sslContext = Client_ssl_init(
    options->caCertFilePath, options->caCertDirPath,
    error, sizeof(error)
  );
  if (!client->sslContext) {
    fprintf(stderr, "❌ Error: %s\n", error);
    Client_destroy(&client);
    return NULL;
  }
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
  struct addrinfo options = {.ai_socktype = SOCK_STREAM};
  struct addrinfo *bindAddress;
  if (getaddrinfo(host, port, &options, &bindAddress)) {
    fprintf(
      this->err, "❌ Invalid IP/port configuration: %s\n",
      gai_strerror(SOCKET_getErrorNumber())
    );
    return false;
  }

  // Get a string representation of the host and port
  char addressBuffer[100] = {};
  char serviceBuffer[100] = {};
  if (getnameinfo(
    bindAddress->ai_addr, bindAddress->ai_addrlen,
    addressBuffer, sizeof(addressBuffer),
    serviceBuffer, sizeof(serviceBuffer),
    NI_NUMERICHOST | NI_NUMERICSERV
  )) {
    fprintf(
      this->err, "getnameinfo() failed (%d): %s\n",
      SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber())
    );
    return false;
  }

  // Try to create the socket
  this->server = socket(
    bindAddress->ai_family, bindAddress->ai_socktype, bindAddress->ai_protocol
  );
  if (!SOCKET_isValid(this->server)) {
    fprintf(
      this->err, "socket() failed (%d): %s\n",
      SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
    );
    return false;
  }

  // Try to connect
  fprintf(this->err, "Connecting to %s:%s...", addressBuffer, serviceBuffer);
  if (connect(this->server, bindAddress->ai_addr, bindAddress->ai_addrlen)) {
    fprintf(
      this->err,
      "FAILED!\n❌ Error: %d - %s\nCheck that the host name and port number are correct\n",
      SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
    );
    return false;
  }
  freeaddrinfo(bindAddress); // We don't need it anymore

  // Wrap the connection into an SSL tunnel
  this->ssl = SSL_new(this->sslContext);
  if (!this->ssl) {
    fprintf(this->err, "❌ Error: SSL_new() failed.\n");
    return false;
  }
  // Set up host validation for server other than localhost
  // X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS disallows *www or www* certificate DN
  // X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS disallows one.two.mydomain.com
  // while allowing first level subdomains like sub.mydomain.com
  // See also https://www.openssl.org/docs/man3.0/man3/SSL_set_hostflags.html
  // and https://wiki.openssl.org/index.php/Hostname_validation
  if (!IsLocalhost(addressBuffer)) {
    SSL_set_hostflags(
      this->ssl,
      X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS | X509_CHECK_FLAG_SINGLE_LABEL_SUBDOMAINS
    );
    if (!SSL_set1_host(this->ssl, host)) {
      fprintf(this->err, "❌ Error: SSL_set1_host() failed.\n");
      return false;
    }
  }
  SSL_set_fd(this->ssl, this->server);
  int connected;
  while (true) {
    connected = SSL_connect(this->ssl);
    if (connected <= 0) {
      switch (SSL_get_error(this->ssl, connected)) {
        case SSL_ERROR_NONE:
        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_ACCEPT:
        case SSL_ERROR_WANT_X509_LOOKUP:
        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
          continue; // with the SSL_connect() loop
        default:
          {
            char error[256] = {};
            ERR_error_string_n(ERR_get_error(), error, sizeof(error));
            fprintf(this->err, "FAILED!\n ❌ SSL_connect() failed: %s\n", error);
            return false;
          }
      }
    }
    // If we are here, we are connected
    break; // out of the SSL_connect() loop
  }
  fprintf(this->err, "OK!\n\n");
  fprintf(this->err, "🔐 SSL/TLS using %s\n", SSL_get_cipher(this->ssl));

  // Download certificate
  X509 *cert = SSL_get_peer_certificate(this->ssl);
  if (cert) {
    // Display certificate details
    char *cInfo;
    if ((cInfo = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0))) {
      fprintf(this->err, "   ⁃subject: %s\n", cInfo);
      OPENSSL_free(cInfo);
    }
    if ((cInfo = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0))) {
      fprintf(this->err, "   ⁃issuer : %s\n", cInfo);
      OPENSSL_free(cInfo);
    }
    // Cleanup
    X509_free(cert);
  } else {
    fprintf(this->err, "   ❌ SSL_get_peer_certificate() failed.\n");
    return false;
  }

  // Wait for the OK signal from the server
  // We are using select() with a timeout here, because if the server
  // doesn't have available connection slots, the client will remain hung.
  // To avoid this, we allow some second for the server to reply before failing
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
      fprintf(
        this->err, "Client select() failed. (%d): %s\n",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      return false;
    }
    if (result == 0) {
      fprintf(this->err, "Client timeout expired\n");
      return false;
    }

    if (FD_ISSET(this->server, &reads)) {
      int received = Client_receive(this);
      if (received < 0) {
        return false;
      }
      // At this point the server will only send /ok or /err messages
      C2HMessage *response = C2HMessage_get(&(this->buffer));
      if (response == NULL) return false;

      if (response->type != kMessageTypeOk) {
        fprintf(this->err, "❌ Error: Connection refused: %s\n", response->content);
        C2HMessage_free(&response);
        return false;
      }
      // Display the server welcome message
      if (strlen(response->content) > 0) {
        fprintf(this->out, "\n💬 %s\n", response->content);
      }
      C2HMessage_free(&response);

      if (!vLogInit(this->logLevel, this->logFilePath)) {
        fprintf(this->err, "Unable to initialise the logger (%s): %s\n", this->logFilePath, strerror(errno));
        fprintf(this->out, "Unable to initialise the logger (%s): %s\n", this->logFilePath, strerror(errno));
        return false;
      }
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
 * Returns the raw client buffer
 *
 * This is handy when external application need to implement
 * a custom listen loop
 */
void *Client_getBuffer(C2HatClient *this) {
  if (this != NULL) return &(this->buffer);
  return NULL;
}

/**
 * Receives data from the server through the client's socket
 * until a null terminator is found or the buffer is full
 *
 * @param[in] this C2HatClient structure holding the connection information
 * @param[out] The number of bytes received
 */
int Client_receive(C2HatClient *this) {
  // Max length of data we can read into the buffer
  size_t length = sizeof(this->buffer.data);
  Debug("Client_receive - max buffer size: %zu", length);

  // Pointer to the end of the buffer, to prevent overflow
  char *eob = this->buffer.data + length -1;

  // Manage buffer status
  if (this->buffer.start != this->buffer.data && *eob != 0) {
    // There is leftover data from a previous read
    size_t remainingTextSize = eob - this->buffer.start + 1;
    // Move the leftover data at the beginning of the buffer...
    memcpy(this->buffer.data, this->buffer.start, remainingTextSize);
    // ...and pad the rest with NULL terminators
    for (size_t i = length; i >= remainingTextSize; i--) {
      *(this->buffer.data + i) = 0;
    }
    // Set the buffer to start reading at the end of the leftover data
    this->buffer.start = this->buffer.data + remainingTextSize + 1;
    length = eob - this->buffer.start + 1; // new buffer start
    Debug("Client_receive - remaining text size: %zu", remainingTextSize);
    Debug("Client_receive - new max buffer size: %zu", length);
  } else {
    // buffer.start != NULL: all data has been already read, no leftover
    // buffer.start == NULL: newly created buffer
    // In either case, reset everything
    this->buffer.start = this->buffer.data;
    memset(this->buffer.data, 0, length); // Reset buffer
  }

  Debug("Client_receive - starting at: %zu", this->buffer.start - this->buffer.data);
  while (true) {
    int bytesReceived = SSL_read(this->ssl, this->buffer.start, length);
    Debug("Client_receive - received (%d bytes): %.*s", bytesReceived, bytesReceived, this->buffer.start);

    if (bytesReceived == 0) {
      fprintf(this->err, "Client_receive - Connection closed by remote server\n");
      return -1;
    }

    if (bytesReceived < 0) {
      // Ignore a signal received before timeout, will be managed by handler
      if (SOCKET_getErrorNumber() == EINTR || EAGAIN == SOCKET_getErrorNumber() || EWOULDBLOCK == SOCKET_getErrorNumber()) continue;

      fprintf(
        this->err, "Client_receive - recv() failed (%d): %s\n",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      return -1;
    }
    // Got data
    if (bytesReceived > 0 ) {
      return bytesReceived;
    }
  }
}

/**
 * Sends data through the client's socket using a loop
 * to ensure all the given data is sent
 *
 * @param[in] this C2HatClient structure holding the connection information
 * @param[in] message The C2HMessage object to send
 * @param[out] Number of bytes sent, -1 on error
 */
int Client_send(const C2HatClient *this, const C2HMessage *message) {
  if (this == NULL) {
    Error("Invalid client instance");
    return -1;
  }
  if (message == NULL) {
    Error("Invalid message");
    return -1;
  }

  size_t total = 0;

  char buffer[kBufferSize] = {};
  size_t length = C2HMessage_format(message, buffer, sizeof(buffer));

  // Cursor pointing to the beginning of the message
  char *data = (char*)buffer;

  Debug("Client_send - about to send (%zu): %s", length, data);

  // Keep sending data until the buffer is empty
  do {
    int bytesSent = SSL_write(this->ssl, data, length - total);
    if (bytesSent < 0) {
      fprintf(
        this->err, "send() failed. (%d): %s\n",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      return -1;
    }
    // Ignoring socket closed (byteSent == 0) on send, will be caught by recv()

    // Cursor now points to the remaining data to be sent
    data += bytesSent;
    total += bytesSent;
  } while (total < length);
  Debug("Client_send - sent %zu bytes", total);
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
    fprintf(
      this->out,
      "❌ Error: Invalid user nickname\nNicknames must be at least 2 characters long\n"
    );
    Client_disconnect(this);
    return false;
  }
  // Wait for the AUTH signal from the server,
  // we are ok for this to be blocking because
  // the server won't sent any data to unauthenticated clients
  int received = Client_receive(this);
  if (received < 0) {
    Client_disconnect(this);
    return false;
  }
  C2HMessage *response = C2HMessage_get(&(this->buffer));
  if (response == NULL) return false;

  if (response->type != kMessageTypeNick) {
    C2HMessage_free(&response);
    fprintf(
      this->out,
      "❌ Error: Unable to authenticate\nUnknown server response\n"
    );
    Client_disconnect(this);
    return false;
  }
  C2HMessage_free(&response);

  // Send credentials
  C2HMessage *message = C2HMessage_create(kMessageTypeNick, "%s", username);
  if (message == NULL) {
    fprintf(
      this->out,
      "❌ Error: Unable to authenticate\nCannot create credentials payload, please retry later\n"
    );
    Client_disconnect(this);
    return false;
  }
  int sent = Client_send(this, message);
  C2HMessage_free(&message);
  if (sent < 0) {
    fprintf(
      this->out,
      "❌ Error: Unable to authenticate\nCannot send data to the server, please retry later\n"
    );
    Client_disconnect(this);
    return false;
  }

  // Wait for OK/ERR
  received = Client_receive(this);
  if (received < 0) {
    fprintf(
      this->out,
      "❌ Error: Authentication failed\nCannot receive a response from the server\n"
    );
    Client_disconnect(this);
    return false;
  }

  response = C2HMessage_get(&(this->buffer));
  if (response == NULL) return false;

  if (response->type != kMessageTypeOk) {
    if (response->type == kMessageTypeErr) {
      fprintf(
        this->out, "❌ [Server Error] Authentication failed\n%s\n",
        response->content
      );
    } else {
      fprintf(
        this->out,
        "❌ Error: Authentication failed\nInvalid response from the server\n"
      );
    }
    C2HMessage_free(&response);
    Client_disconnect(this);
    return false;
  }
  C2HMessage_free(&response);
  return true;
}

void Client_disconnect(C2HatClient *this) {
  // Close SSL connection first, then close socket and free memory
  if (this->ssl != NULL) {
    SSL_shutdown(this->ssl);
    if (SOCKET_isValid(this->server)) SOCKET_close(this->server);
    SSL_free(this->ssl);
    this->ssl = NULL;
  }
}

/**
 * Safely destroys a C2HatClient object
 */
void Client_destroy(C2HatClient **this) {
  if (this != NULL) {
    // Check if we need to close the socket
    if (*this != NULL) {
      Client_disconnect(*this);
      SSL_CTX_free((*this)->sslContext);
      if ((*this)->chain != NULL) sk_X509_pop_free((*this)->chain, X509_free);
      // Erase the used memory
      memset(*this, 0, sizeof(C2HatClient));
      free(*this);
      *this = NULL;
    }
  }
}

