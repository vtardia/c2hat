/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "server.h"

#include "message/message.h"
#include "socket/socket.h"
#include "list/list.h"
#include "queue/queue.h"
#include "validate/validate.h"

#include <pthread.h>
#include <wchar.h>

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

enum {
  kMaxClientConnections = 5,
  kMaxClientHostLength = NI_MAXHOST,
  kAuthenticationTimeout = 30, // seconds
  kChatTimeout = 3 * 60 // 3 minutes
};

/// Regex pattern used to validate the user nickname
static const char *kRegexNicknamePattern = "^[[:alpha:]][[:alnum:]!@#$%&]\\{1,14\\}$";

/// Validation error message for invalid user names, includes the rules
static const char *kErrorMessageInvalidUsername = "Nicknames must start with a letter and contain 2-15 latin characters and !@#$%&";

/// Holds data for queued messages
typedef struct {
  char sender[kMaxNicknameSize + sizeof(wchar_t)]; ///< Sender's nickname
  char content[kBufferSize]; ///< Message content
  int  contentLength; ///< Message length
} Message;

/// Holds the details of connected clients
typedef struct {
  pthread_t threadID; ///< Thread ID associated to the client
  char nickname[kMaxNicknameSize + sizeof(wchar_t)]; ///< Client name (+ NULL terminator)
  SOCKET socket; ///< Client's socket
  struct sockaddr_storage address; ///< Binary IP address
  socklen_t length; ///< Length of the binary IP address
  char host[kMaxClientHostLength]; ///< IP address in pretty string format
  SSL *ssl; ///< SSL connection handle
} Client;

/// This is the singleton instance for our server
struct Server {
  char *host; ///< Inbound IP address (string)
  int port;   ///< Inbound TCP port
  int maxConnections; ///< Maximum number of connections accepted
  SOCKET socket; ///< Stores the server socket
  SSL_CTX *ssl; ///< SSL context
};
static Server *server = NULL;

static bool terminate = false;

static List *clients = NULL;
static Queue *messages = NULL;

// Mutex for client list
pthread_mutex_t clientsLock = PTHREAD_MUTEX_INITIALIZER;

// Mutex for message queue
pthread_mutex_t messagesLock = PTHREAD_MUTEX_INITIALIZER;

// Signal handling
int Server_catch(int sig, void (*handler)(int));
void Server_stop(int signal);

// Send and receive data from client sockets
int Server_send(Client *client, const char* message, size_t length);
int Server_receive(Client *client, char *buffer, size_t length);

// Manages a client's thread
void* Server_handleClient(void* data);

// Broadcast messages to all connected clients
void* Server_handleBroadcast(void* data);
void Server_broadcast(char *message, size_t length);

// Closes a client connection and related thread
void Server_dropClient(Client *client);

// Find a Client object in the list given a nickname or a thread id
Client *Server_getClientInfoForThread(pthread_t clientThreadID);
Client *Server_getClientInfoForNickname(char *clientNickname);

// Comparison functions for List_search
int Client_findByThreadID(const ListData *a, const ListData *b, size_t size);
int Client_findByNickname(const ListData *a, const ListData *b, size_t size);

// Authenticate a client connection using a nickname
bool Server_authenticate(Client *client);

/**
 * Creates and initialises the server object
 * @param[in] config A valid ServerConfigInfo structure
 */
Server *Server_init(ServerConfigInfo *config) {
  if (server != NULL) {
    Fatal("The server process is already running");
  }

  // Hold socket settings
  struct addrinfo options = {};

  // Hold the return value for getaddrinfo()
  struct addrinfo *bindAddress;

  // Hold the server port number as string
  char serverPort[6] = {};
  sprintf(serverPort, "%d", config->port);

  // AF_INET = accepts only IPv4 host values
  // AF_INET6 = accepts only IPv6 host values
  // PF_UNSPEC = accepts any IP types depending on system configuration
  options.ai_family = PF_UNSPEC;
  options.ai_socktype = SOCK_STREAM; // TCP connections
  // AI_PASSIVE = socket used for listening
  // AI_ADDRCONFIG = if receives IPv4 returns IPv4, if receives IPv6 returns IPv6
  options.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;

  // Returns a binary IP address representation for bind()
  if (getaddrinfo(config->host, serverPort, &options, &bindAddress)) {
    Fatal("Invalid IP/port configuration: %s", gai_strerror(SOCKET_getErrorNumber()));
  }

  // Initialise OpenSSL
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();

  SSL_CTX *sslContext = SSL_CTX_new(TLS_server_method());
  if (!sslContext) {
    Fatal("SSL_CTX_new() failed: cannot create SSL context");
  }
  // Set minimum protocol to TLS 1.2, recommended for internal or buiness apps
  // Use TLS 1.0 or 1.1 for wider compatibility while still keeping
  // a good level of security
  // See also https://developers.cloudflare.com/ssl/edge-certificates/additional-options/minimum-tls
  if (!SSL_CTX_set_min_proto_version(sslContext, TLS1_2_VERSION)) {
    SSL_CTX_free(sslContext);
    Fatal("Cannot set minimum TLS protocol version");
  }
  // See https://www.openssl.org/docs/man3.0/man3/SSL_CTX_set_options.html
  SSL_CTX_set_options(sslContext, SSL_OP_ALL | SSL_OP_NO_RENEGOTIATION);
  // See https://www.openssl.org/docs/man3.0/man3/SSL_CTX_set_mode.html
  SSL_CTX_set_mode(
    sslContext,
    SSL_MODE_AUTO_RETRY | SSL_MODE_ENABLE_PARTIAL_WRITE | SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER
  );
  // Set verification rules
  // SSL_VERIFY_NONE on server means that the client will not be required
  // to send a client certificate
  // See https://www.openssl.org/docs/man3.0/man3/SSL_CTX_set_verify.html
  SSL_CTX_set_verify(sslContext, SSL_VERIFY_NONE, NULL);
  // See https://www.openssl.org/docs/man3.0/man3/SSL_CTX_set_cipher_list.html
  SSL_CTX_set_cipher_list(
    sslContext,
    "TLS_AES_256_GCM_SHA384:TLS_CHACHA20_POLY1305_SHA256:TLS_AES_128_GCM_SHA256"
  );

  if (!SSL_CTX_use_certificate_chain_file(sslContext, config->sslCertFilePath)
    || !SSL_CTX_use_PrivateKey_file(sslContext, config->sslKeyFilePath, SSL_FILETYPE_PEM)) {
    char error[256] = {};
    ERR_error_string_n(ERR_get_error(), error, sizeof(error));
    SSL_CTX_free(sslContext);
    Fatal("SSL_CTX_use_certificate_file() failed: %s", error);
  }
  if (!SSL_CTX_check_private_key(sslContext)) {
    SSL_CTX_free(sslContext);
    Fatal("Private key does not match the public certificate");
  }

  // Create a server instance
  server = (Server *)calloc(sizeof(Server), 1);
  server->ssl = sslContext;
  server->socket = Socket_new(bindAddress->ai_family, bindAddress->ai_socktype, bindAddress->ai_protocol);
  Socket_unsetIPV6Only(server->socket);
  Socket_setReusableAddress(server->socket);
  Socket_setNonBlocking(server->socket);

  Socket_bind(server->socket, bindAddress->ai_addr, bindAddress->ai_addrlen);

  // Note: if the bind fails here (e.g. server is already running),
  // the memory allocated for the server and bindAddress vars will not be freed

  // We don't need bindAddress anymore
  freeaddrinfo(bindAddress);

  Socket_listen(server->socket, config->maxConnections);
  server->host = strdup(config->host);
  server->port = config->port;
  server->maxConnections = config->maxConnections;

  return server;
}

/**
 * Frees the memory allocated for the server object
 * @param[in] this Double pointer to a server structure
 */
void Server_free(Server **this) {
  if (this != NULL) {
    free((*this)->host);
    SSL_CTX_free((*this)->ssl);
    memset(*this, 0, sizeof(Server));
    free(*this);
    *this = NULL;
  }
}

/**
 * Manages SIGINT and SIGTERM by setting the server termination flag
 * @param[in] signal The received signal interrupt
 */
void Server_stop(int signal) {
  char *sig_name = strsignal(signal);
  Info("Signal '%s' received, shutting down...", sig_name);
  terminate = true;
}

/**
 * Catch interrupt signals
 * @param[in] sig The signal to manage
 * @param[in] handler Pointer to the function that will manage the signal
 * @param[out] The result of the sigaction() call
 */
int Server_catch(int sig, void (*handler)(int)) {
   struct sigaction action = {
     .sa_handler = handler,
     // We also want to unset the SA_RESTART flag for the handler(s),
     // which would make accept() return -1 on the signal's reception.
     .sa_flags = 0
   };
   sigemptyset(&action.sa_mask);
   return sigaction (sig, &action, NULL);
}

/**
 * Starts the server instance with the given configuration
 * @param[in] this The server object to start
 */
void Server_start(Server *this) {
  if (server == NULL) {
    Fatal("The server has not been created yet");
  }

  // Stop from Ctrl+C
  Server_catch(SIGINT, Server_stop);

  // Stop from System or control process
  Server_catch(SIGTERM, Server_stop);

  // Ignoring SIGPIPE (= sending data to a closed socket)
  Server_catch(SIGPIPE, SIG_IGN);

  // Create a thread for broadcast messages
  pthread_t broadcastThreadID = 0;
  pthread_create(&broadcastThreadID, NULL, Server_handleBroadcast, NULL);

  clients = List_new();
  if (clients == NULL) {
    Fatal("Unable to initialise clients list");
  }

  messages = Queue_new();
  if (messages == NULL) {
    Fatal("Unable to initialise message queue");
  }

  fd_set reads;
  fd_set errors;
  FD_ZERO(&reads);
  FD_ZERO(&errors);
  FD_SET(this->socket, &reads);
  FD_SET(this->socket, &errors);
  SOCKET maxSocket = this->socket;

  while (!terminate) {

    if (select(maxSocket+1, &reads, 0, &errors, 0) < 0) {
      if (EINTR == SOCKET_getErrorNumber()) {
        // Signal received
        Info("%s", strerror(SOCKET_getErrorNumber()));
      } else {
        Error("select() failed (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      }
      continue;
    }

    // Main socket is ready to read
    if (FD_ISSET(this->socket, &reads)) {

      // Initialise a temporary client variable
      Client client = {};
      client.length = sizeof(client.address);
      client.socket = accept(this->socket, (struct sockaddr*) &(client).address, &(client).length);
      if (!SOCKET_isValid(client.socket)) {
        if (EINTR == SOCKET_getErrorNumber()) {
          Info("%s", strerror(SOCKET_getErrorNumber()));
        } else if (EWOULDBLOCK != SOCKET_getErrorNumber()) {
          Error("accept() failed (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
        }
        continue;
      }

      // Try to start an SSL connection
      client.ssl = SSL_new(server->ssl);
      if (!client.ssl) {
        Error("SSL_new() failed: cannot open an SSL client connection");
        continue;
      }
      SSL_set_fd(client.ssl, client.socket);
      int accepted;
      while (true) {
        accepted = SSL_accept(client.ssl);
        if (accepted != 1) {
          if (accepted == 0) {
            Error("SSL_accept(): connection closed clean");
            Server_dropClient(&client);
            break; // out of the SSL_accept() loop
          }
          switch(SSL_get_error(client.ssl, accepted)) {
            case SSL_ERROR_WANT_READ:
            case SSL_ERROR_WANT_WRITE:
              continue; // the SSL_accept() loop
            default:
              {
                char error[256] = {};
                ERR_error_string_n(ERR_get_error(), error, sizeof(error));
                Error("SSL_accept() failed: %s", error);
                Server_dropClient(&client);
              }
            }
          break; // out of the SSL_accept() loop
        }
        // If we are here, the connection has been accepted
        break; // out of the SSL_accept() loop
      }
      if (accepted != 1) continue;

      // A client has connected, log the client info
      getnameinfo(
        (struct sockaddr*)&(client).address,
        client.length, client.host, kMaxClientHostLength,
        0, 0,
        NI_NUMERICHOST
      );
      Info("New connection from %s", client.host);
      Info("SSL connection using %s", SSL_get_cipher(client.ssl));

      pthread_t clientThreadID = 0;
      if (clients->length < kMaxClientConnections) {

        // Add client to the list (the client is cloned)
        pthread_mutex_lock(&clientsLock);
        List_append(clients, &client, sizeof(Client));

        // Get pointer to a copy of the inserted client,
        // because the temporary client var will be overridden
        // at the beginning of the next cycle
        Client *last = (Client *)List_last(clients);

        // Start client thread
        pthread_create(&clientThreadID, NULL, Server_handleClient, last);
        last->threadID = clientThreadID; // Update client list item
        pthread_mutex_unlock(&clientsLock);
        pthread_detach(clientThreadID);
      } else {
        char *err = "/err connection limits reached";
        Server_send(&client, err, strlen(err) + 1);
        Info("Connection limits reached", err);
        Server_dropClient(&client);
      }
    }

    // Main socket has an error
    if (FD_ISSET(this->socket, &errors)) {
      Error(
        "Main socket failed (%d): %s",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
    }
  }

  Info("Terminating...");

  // Wait for all client threads to exit
  Client *client;
  while ((client = (Client *)List_next(clients)) != NULL) {
    pthread_join(client->threadID, NULL);
  }
  // Destroy client list
  List_free(&clients);

  // Close broadcast thread
  pthread_join(broadcastThreadID, NULL);
  Queue_free(&messages);

  // Cleanup socket and server
  SOCKET_close(this->socket);
  FD_CLR(this->socket, &reads);
  FD_CLR(this->socket, &errors);
  Server_free(&server);
}

/**
 * Sends data to a socket using a loop to ensure all data is sent
 * @param[in] client The Client object containing a valid socket
 * @param[in] message The message to send
 * @param[in] length The length of the message to send
 */
int Server_send(Client *client, const char* message, size_t length) {
  size_t sentTotal = 0;
  char *data = (char*)message; // points to the beginning of the message
  do {
    if (!SOCKET_isValid(client->socket)) return -1;
    int sent = SSL_write(client->ssl, data, length - sentTotal);
    if (sent < 0) {
      if (0 != SOCKET_getErrorNumber()) Error(
        "send() failed: (%d): %s",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      return -1;
    }
    data += sent; // points to the remaining data to be sent
    sentTotal += sent;
  } while (sentTotal < length);
  return sentTotal;
}

/**
 * Comparison function to lookup a client by its ThreadID
 * @param[in] a Pointer to a Client struct object
 * @param[in] b Pointer to a Thread ID
 * @param[in] size (Unused) Size of the data to compare
 * @param[out] 0 on equal Thread IDs, non-zero otherwise
 */
int Client_findByThreadID(const ListData *a, const ListData *b, size_t size) {
  Client *client = (Client *)a;
  (void)size; // Avoids the 'unused parameter' error at compile time
  pthread_t *threadB = (pthread_t *)b;
  return (unsigned long)client->threadID - (unsigned long)*threadB;
}

/**
 * Comparison function to lookup a client by its Nickname
 * @param[in] a Pointer to a Client struct object
 * @param[in] b Char pointer, with the nickname to compare
 * @param[in] size (Unused) Size of the data to compare
 * @param[out] 0 on equal nicknames, non-zero otherwise
 */
int Client_findByNickname(const ListData *a, const ListData *b, size_t size) {
  Client *client = (Client *)a;
  char *nickname = (char *)b;
  (void)size;
  return strncmp(client->nickname, nickname, kMaxNicknameSize);
}

/**
 * Removes a client object from the list of connected clients
 * and closes the connection
 * @param[in] client The client structure that contains the socket
 */
void Server_dropClient(Client *client) {
  if (!client->threadID) {
    // The client doesn't have a thread, which means
    //  - the SSL connection was denied by some error
    //  - there are no available connections
    Info("Dropping threadless client");
    SSL_shutdown(client->ssl);
    SOCKET_close(client->socket);
    SSL_free(client->ssl);
    return;
  }

  // The client is a regularly authenticated client
  pthread_t clientThreadID = client->threadID;

  // Drop client from the clients list
  // We need to pass sizeof(Client) as size or the item will not be compared
  pthread_mutex_lock(&clientsLock);
  int index = List_search(clients, &clientThreadID, sizeof(Client), Client_findByThreadID);
  if (index >= 0) {
    // Close client socket here, or it will hang during broadcast
    // with a bad file descriptor error
    SSL_shutdown(client->ssl);
    SOCKET_close(client->socket);
    SSL_free(client->ssl);
    if (!List_delete(clients, index)) {
      Warn("Unable to drop client %d with thread ID %lu", index, clientThreadID);
    }
  } else {
    Warn("Unable to find client with thread ID %lu", clientThreadID);
  }
  pthread_mutex_unlock(&clientsLock);
  Info("Closing client thread %lu", clientThreadID);
  pthread_exit(NULL);
}

/**
 * Lookup a Client in the list by its thread ID
 * @param[in] clientThreadID Thread ID to lookup
 * @param[out] A pointer to a Client structure or NULL
 */
Client *Server_getClientInfoForThread(pthread_t clientThreadID) {
  Client *client = NULL;
  pthread_mutex_lock(&clientsLock);
  int index = List_search(clients, &clientThreadID, sizeof(Client), Client_findByThreadID);
  if (index >= 0) {
    client = (Client *)List_item(clients, index);
  }
  pthread_mutex_unlock(&clientsLock);
  return client;
}

/**
 * Lookup a Client in the list by its nickname
 * @param[in] clientNickname Nickname to lookup
 * @param[out] A pointer to a Client structure or NULL
 */
Client *Server_getClientInfoForNickname(char *clientNickname) {
  Client *client = NULL;
  pthread_mutex_lock(&clientsLock);
  int index = List_search(clients, clientNickname, sizeof(Client), Client_findByNickname);
  if (index >= 0) {
    client = (Client *)List_item(clients, index);
  }
  pthread_mutex_unlock(&clientsLock);
  return client;
}

/**
 * Receives a message from a connected client until
 * a null terminator is found or the buffer is full
 * @param[in] client Client struct containing the socket to receive from
 * @param[in] buffer Char buffer to store the received data
 * @param[in] length Length of the char buffer
 * @param[out] The number of bytes received
 */
int Server_receive(Client *client, char *buffer, size_t length) {
  char *data = buffer; // points at the start of buffer
  size_t total = 0;
  char cursor[1] = {};
  do {
    int bytesReceived = SSL_read(client->ssl, cursor, 1);

    // The remote client closed the connection
    if (bytesReceived == 0) return 0;

    // There has been an error somewhere
    if (bytesReceived < 0 ) {
      // The socket is non-blocking
      if (EAGAIN == SOCKET_getErrorNumber() || EWOULDBLOCK == SOCKET_getErrorNumber()) {
        continue;
      } else {
        return total;
      }
    }

    *data = cursor[0];
    data++;
    total++;
    if (total == (length - 1)) break;
  } while(cursor[0] != 0);

  // Adding safe terminator in case of loop break
  if (*data != 0) *(data + 1) = 0;

  return total;
}
/**
 * Validates a username with a pre-defined regex
 * @param[in]  username
 * @param[out] success/failure
 */
bool Client_nicknameIsValid(const char *username) {
  char error[512] = {};
  int valid = Regex_match(username, kRegexNicknamePattern, error, sizeof(error));
  if (valid) return true;
  if (valid < 0) {
    // There has been an error in either compiling
    // or executing the regex above, we log it for investigation
    Error("Unable to validate username '%s': %s", username, error);
  }
  return false;
}

/**
 * Authenticates a client connection
 * Currently it only ensures that another client is not already connected
 * using the same nickname, and the client entry is in the clients list
 * @param[in] client Client object containing a socket to communicate with
 * @param[out] Success or failure
 */
bool Server_authenticate(Client *client) {
  char request[kBufferSize] = {};
  char response[kBufferSize] = {};
  struct timeval timeout;

  Message_format(kMessageTypeNick, request, kBufferSize, "Please enter a nickname:");
  Server_send(client, request, strlen(request) + 1);

  fd_set reads;
  fd_set errors;
  FD_ZERO(&reads);
  FD_ZERO(&errors);
  FD_SET(client->socket, &reads);
  FD_SET(client->socket, &errors);
  SOCKET maxSocket = client->socket;

  timeout.tv_sec  = kAuthenticationTimeout;
  timeout.tv_usec = 0;

  while(true) {
    int rc = select(maxSocket + 1, &reads, 0, &errors, &timeout);
    if (rc < 0) {
      if (EINTR == SOCKET_getErrorNumber()) {
        // System interrupt signal
        Info("%s", strerror(SOCKET_getErrorNumber()));
      } else {
        Error(
          "select() failed on authentication (%d): %s",
          SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
        );
      }
      continue;
    }

    // Timeout expired
    if (rc == 0) {
      memset(request, '\0', kBufferSize);
      Message_format(kMessageTypeErr, request, kBufferSize, "Authentication timeout expired!");
      Server_send(client, request, strlen(request) + 1);
      break;
    }

    // Socket ready to receive
    if (FD_ISSET(client->socket, &reads)) {
      int received = Server_receive(client, response, kBufferSize);
      if (received > 0) {
        if (kMessageTypeNick == Message_getType(response)) {
          // The client has sent a nick in the format '/nick Name'
          // In order to have a full 20 chars nickname, we need to add a 7 chars pad
          // to the length: 5chars for the /nick prefix, + 1 space + null-terminator
          char *nick = Message_getContent(response, kMessageTypeNick, kMaxNicknameSize + 7);

          // User name validation
          if (!Client_nicknameIsValid(nick)) {
            memset(request, '\0', kBufferSize);
            Message_format(kMessageTypeErr, request, kBufferSize, kErrorMessageInvalidUsername);
            Server_send(client, request, strlen(request) + 1);
            Message_free(&nick);
            break;
          }

          Client *clientInfo = NULL;
          // Lookup if a client is already logged with the provided nickname
          clientInfo = Server_getClientInfoForNickname(nick);
          if (clientInfo == NULL) {
            // The user's nickname is unique
            // Lookup client by thread
            clientInfo = Server_getClientInfoForThread(pthread_self());
            if (clientInfo != NULL && clientInfo == client) {
              // Update client entry
              snprintf(clientInfo->nickname, kMaxNicknameSize, "%s", nick);
              Info(
                "User %s (%d bytes) authenticated successfully!",
                clientInfo->nickname, strlen(clientInfo->nickname)
              );
              Message_free(&nick);
              return true;
            }
            Error("Authentication: client info not found for client %lu", pthread_self());
            Message_free(&nick);
            return false;
          }
          Info("Client with nick '%s' is already logged in", nick);
          Message_free(&nick);
        }
      }
    }

    // Client socket has an error
    if (FD_ISSET(client->socket, &errors)) {
      Error(
        "Client socket failed during authentication (%d): %s",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      break;
    }

    if (!SOCKET_isValid(client->socket)) break;
  }
  // We leave error management or client disconnection to the calling function
  return false;
}


/**
 * Handles communications with a single client, it is spawned
 * on a new thread for every connected client
 * @param[in] data Pointer to a Client structure
 */
void* Server_handleClient(void* data) {
  // This should be = to client->threadID
  pthread_t me = pthread_self();
  Client *client = (Client *)data;
  if (me != client->threadID) {
    Error("Client thread id mismatch (client: %lu, me: %lu)", client->threadID, me);
    Server_dropClient(client);
  }
  char messageBuffer[kBufferSize] = {};

  Info("Starting new client thread %lu", client->threadID);

  // Send a welcome message
  Message_format(kMessageTypeOk, messageBuffer, kBufferSize, "Welcome to C2hat!");
  // Using strlen() +1 ensures the NULL terminator is sent
  Server_send(client, messageBuffer, strlen(messageBuffer) + 1);

  // Ask for a nickname
  if (!Server_authenticate(client)) {
    Info("Authentication failed for client thread %lu", client->threadID);
    memset(messageBuffer, '\0', kBufferSize);
    Message_format(kMessageTypeErr, messageBuffer, kBufferSize, "Authentication failed");
    Server_send(client, messageBuffer, strlen(messageBuffer) + 1);
    Server_dropClient(client);
  }

  // Say Hello to the new user
  memset(messageBuffer, '\0', kBufferSize);
  Message_format(kMessageTypeOk, messageBuffer, kBufferSize, "Hello %s!", client->nickname);
  Server_send(client, messageBuffer, strlen(messageBuffer) + 1);

  // Broadcast that a new client has joined
  memset(messageBuffer, '\0', kBufferSize);
  Message_format(
    kMessageTypeLog, messageBuffer, kBufferSize,
    "[%s] just joined the chat", client->nickname
  );
  Server_broadcast(messageBuffer, strlen(messageBuffer) + 1);

  fd_set reads;
  fd_set errors;
  FD_ZERO(&reads);
  FD_ZERO(&errors);
  FD_SET(client->socket, &reads);
  FD_SET(client->socket, &errors);
  SOCKET maxSocket = client->socket;
  struct timeval timeout;
  timeout.tv_sec  = kChatTimeout;
  timeout.tv_usec = 0;

  // Start the chat
  while(!terminate) {
    // Initialise buffer for client data
    memset(messageBuffer, '\0', kBufferSize);

    int rc = select(maxSocket + 1, &reads, 0, &errors, &timeout);
    if (rc < 0) {
      if (EINTR == SOCKET_getErrorNumber()) {
        Info("%s", strerror(SOCKET_getErrorNumber()));
      } else {
        Error("select() failed (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      }
      continue;
    }

    // Timeout expired
    if (rc == 0) {
      Message_format(
        kMessageTypeErr, messageBuffer, kBufferSize,
        "Connection timed out, you've been disconnected!"
      );
      Server_send(client, messageBuffer, strlen(messageBuffer) + 1);
      break;
    }

    // Main socket is ready to read
    if (FD_ISSET(client->socket, &reads)) {

      // Listen for data
      int received = Server_receive(client, messageBuffer, kBufferSize);
      if (received < 0) {
        if (SOCKET_getErrorNumber() == 0) {
          Info("Connection closed by remote client (1) %d", ECONNRESET);
        } else {
          Error("recv() failed: (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
        }
        break;
      }

      if (received == 0) {
        Info("Connection closed by remote client (2) %d", ECONNRESET);
        break;
      }

      if (received > 0) {
        Info("Received: (%d bytes) %.*s", received, received, messageBuffer);

        int messageType = Message_getType(messageBuffer);
        if (kMessageTypeQuit == messageType) break;

        char *messageContent = Message_getContent(messageBuffer, kMessageTypeMsg, kBufferSize);
        char broadcastBuffer[kBroadcastBufferSize] = {};
        switch (messageType) {
          case kMessageTypeMsg:
            if (messageContent != NULL && strlen(messageContent) > 0) {

              // Send /ok to the client to acknowledge the correct message
              memset(messageBuffer, '\0', kBufferSize);
              Message_format(kMessageTypeOk, messageBuffer, kBufferSize, "");
              if (Server_send(client, messageBuffer, strlen(messageBuffer) + 1) < 0) break;

              // Broadcast the message to all clients using the format '/msg [<20charUsername>]: ...'
              Message_format(
                kMessageTypeMsg, broadcastBuffer, kBroadcastBufferSize,
                "[%s] %s", client->nickname, messageContent
              );
              Server_broadcast(broadcastBuffer, strlen(broadcastBuffer) + 1);
              Message_free(&messageContent);
            }
          break;
          default:
            ; // Ignore for now...
        }
      }
    }

    // Client socket has an error
    if (FD_ISSET(client->socket, &errors)) {
      Error(
        "Client socket failed (%d): %s",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
    }

    if (!SOCKET_isValid(client->socket)) break;
  }

  // Broadcast that client has left
  memset(messageBuffer, '\0', kBufferSize);
  Message_format(
    kMessageTypeLog, messageBuffer, kBufferSize,
    "[%s] just left the chat", client->nickname
  );
  Server_broadcast(messageBuffer, strlen(messageBuffer) + 1);

  // Close the connection
  Server_dropClient(client);
  FD_CLR(client->socket, &reads);
  FD_CLR(client->socket, &errors);

  return NULL;
}

/**
 * Retrieves a message from the broadcast queue and sends it
 * to every client
 * @param[in] data Unused data pointer, set it to NULL
 */
void* Server_handleBroadcast(void* data) {
  pthread_t me = pthread_self();
  QueueData *item = NULL;

  // Define a sleep interval in milliseconds
  struct timespec ts;
  int msec = 200;
  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  while (!terminate) {
    if (Queue_empty(messages)) {
      nanosleep(&ts, NULL);
      continue;
    }
    do {
      // Lock Message Queue
      pthread_mutex_lock(&messagesLock);
      item = Queue_dequeue(messages);
      // Unlock queue
      pthread_mutex_unlock(&messagesLock);
      if (item == NULL) break; // Queue is empty

      Client *client;

      // Lock clients
      pthread_mutex_lock(&clientsLock);

      List_rewind(clients);
      while ((client = (Client *)List_next(clients)) != NULL) {
        // Don't broadcast messages to non-authenticated clients
        if (strlen(client->nickname) == 0) continue;
        // The client may have been disconnected with a /quit message
        // the server will hang if tries to send a message
        if (SOCKET_isValid(client->socket)) {
          int sent = Server_send(client, (char*)item->content, item->length);
          if (sent <= 0) Server_dropClient(client);
        }
      }
      QueueData_free(&item);

      // Unlock clients
      pthread_mutex_unlock(&clientsLock);
    } while(!Queue_empty(messages));
  }
  Info("Closing broadcast thread %lu", me);
  pthread_exit(data);
}

/**
 * Adds a message to the broadcast queue
 * @param[in] message Message to enqueue
 * @param[in] length Size of the data to enqueue
 */
void Server_broadcast(char *message, size_t length) {
  pthread_mutex_lock(&messagesLock);
  Queue_enqueue(messages, message, length);
  pthread_mutex_unlock(&messagesLock);
}

