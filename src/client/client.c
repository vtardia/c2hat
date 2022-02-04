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

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "socket/socket.h"
#include "message/message.h"

#include "../c2hat.h"

/// Contains information on the current client application
typedef struct _C2HatClient {
  SOCKET server;         ///< Connection socket resource
  FILE *in;              ///< Input stream (currently unused)
  FILE *out;             ///< Output stream
  FILE *err;             ///< Error stream
  SSL_CTX *sslContext;   ///< SSL context resource
  SSL *ssl;              ///< SSL connection handle
  STACK_OF(X509) *chain; ///< SSL certificate chain from the server
} C2HatClient;

/// Keeps track of SSL initialisation that should happen only once
static bool sslInit = false;

/**
 * Initialises the OpenSSL library functions
 */
SSL_CTX *Client_ssl_init(char *error, size_t length) {
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

  // Load CA certificates locations
  // TODO this works on Linux, find a way to make it configurable for Mac and Win
  char * caCertDir = "/etc/ssl/certs/";
  char * caCertFile = "/etc/ssl/certs/ca-certificates.crt";
  if (!SSL_CTX_load_verify_locations(context, caCertFile, caCertDir)) {
    strncpy(error, "Unable to load CA locations", length);
    return NULL;
  }
  // Tell OpenSSL to autometically check the server certificate or fail
  SSL_CTX_set_verify(context, SSL_VERIFY_PEER, NULL);
  return context;
}

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

  char error[100] = {0};
  client->sslContext = Client_ssl_init(error, 100);
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
  struct addrinfo options;
  memset(&options, 0, sizeof(options));
  struct addrinfo *bindAddress;
  options.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(host, port, &options, &bindAddress)) {
    fprintf(
      this->err, "❌ Invalid IP/port configuration: %s\n",
      gai_strerror(SOCKET_getErrorNumber())
    );
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
  bool isLocalHost = (strcmp(addressBuffer, "127.0.0.1") == 0 || strcmp(addressBuffer, "::1") == 0);
  if (!isLocalHost) {
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
  if (SSL_connect(this->ssl) < 0) {
    char error[256] = {0};
    ERR_error_string_n(ERR_get_error(), error, 256);
    fprintf(this->err, "FAILED!\n ❌ SSL_connect() failed: %s\n", error);
    return false;
  }
  fprintf(this->err, "OK!\n\n");
  fprintf(this->err, "🔐 SSL/TLS using %s\n", SSL_get_cipher(this->ssl));

  // Download certificate
  X509 *cert = SSL_get_peer_certificate(this->ssl);
  if (cert) {
    // Display certificate details
    char *cInfo;
    if ((cInfo = X509_NAME_oneline(X509_get_subject_name(cert),0,0))) {
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
      int received = Client_receive(this, buffer, kBufferSize);
      if (received < 0) {
        return false;
      }
      // At this point the server will only send /ok or /err messages
      if (Message_getType(buffer) != kMessageTypeOk) {
        fprintf(this->err, "❌ Error: Connection refused by the chat server\n");
        return false;
      }
      // Display the server welcome message
      char *messageContent = Message_getContent(buffer, kMessageTypeOk, received);
      if (strlen(messageContent) > 0) {
        fprintf(this->err, "\n💬 %s\n", messageContent);
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
    int bytesReceived = SSL_read(this->ssl, cursor, 1);
    if (bytesReceived == 0) {
      fprintf(this->err, "Connection closed by remote server\n");
      return -1;
    }
    if (bytesReceived < 0) {
      // Ignore a signal received before timeout, will be managed by handler
      if (SOCKET_getErrorNumber() == EINTR) continue;

      fprintf(
        this->err, "recv() failed. (%d): %s\n",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
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
      this->err,
      "❌ Error: Invalid user nickname\nNicknames must be at least 2 characters long\n"
    );
    Client_disconnect(this);
    return false;
  }
  // Wait for the AUTH signal from the server,
  // we are ok for this to be blocking because
  // the server won't sent any data to unauthenticated clients
  char buffer[kBufferSize] = {0};
  int received = Client_receive(this, buffer, kBufferSize);
  if (received < 0) {
    Client_disconnect(this);
    return false;
  }
  if (Message_getType(buffer) != kMessageTypeNick) {
    fprintf(
      this->err,
      "❌ Error: Unable to authenticate\nUnknown server response\n"
    );
    Client_disconnect(this);
    return false;
  }

  // Send credentials
  char message[kBufferSize] = {0};
  Message_format(kMessageTypeNick, message, kBufferSize, "%s", username);
  int sent = Client_send(this, message, strlen(message) + 1);
  if (sent < 0) {
    fprintf(
      this->err,
      "❌ Error: Unable to authenticate\nCannot send data to the server, please retry later\n"
    );
    Client_disconnect(this);
    return false;
  }

  // Wait for OK/ERR
  memset(buffer, 0, kBufferSize);
  received = Client_receive(this, buffer, kBufferSize);
  if (received < 0) {
    fprintf(
      this->err,
      "❌ Error: Authentication failed\nCannot receive a response from the server\n"
    );
    Client_disconnect(this);
    return false;
  }

  int messageType = Message_getType(buffer);
  if (messageType != kMessageTypeOk) {
    if (messageType == kMessageTypeErr) {
      char *errorMessage = Message_getContent(buffer, kMessageTypeErr, kBufferSize);
      fprintf(
        this->err, "❌ [Server Error] Authentication failed\n%s\n",
        errorMessage
      );
      Message_free(&errorMessage);
    } else {
      fprintf(
        this->err,
        "❌ Error: Authentication failed\nInvalid response from the server\n"
      );
    }
    Client_disconnect(this);
    return false;
  }
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
