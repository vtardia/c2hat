/*
 * Copyright (C) 2020 Vito Tardia
 */

#include "server.h"

#include <pthread.h>

enum {
  kMaxClientConnections = 5,
  kMaxClientHostLength = NI_MAXHOST,
  kMaxNicknameLength = 20,
  kBufferSize = 1024, // Includes NULL term
  // Format is: /msg [<20charUsername>]:\s
  kBroadcastBufferSize = 9 + kMaxNicknameLength + kBufferSize,
};

/// Holds data for queued messages
typedef struct {
  char sender[kMaxNicknameLength + 1]; ///< Sender's nickname
  char content[kBufferSize]; ///< Message content
  int  contentLength; ///< Message length
} Message;

/// Holds the details of connected clients
typedef struct {
  pthread_t threadID; ///< Thread ID associated to the client
  char nickname[kMaxNicknameLength + 1]; ///< Client name (+ NULL terminator)
  SOCKET socket; ///< Client's socket
  struct sockaddr_storage address; ///< Binary IP address
  socklen_t length; ///< Length of the binary IP address
  char host[kMaxClientHostLength]; ///< IP address in pretty string format
} Client;

/// This is the singleton instance for our server
struct Server {
  char *host; ///< Inbound IP address (string)
  int port;   ///< Inbound TCP port
  int maxConnections; ///< Maximum number of connections accepted
  SOCKET socket; ///< Stores the server socket
};
static Server *server = NULL;

static bool terminate = false;

static List *clients = NULL;
static Queue *messages = NULL;

// Initialise, start and destroy the server object
Server *Server_init(const char *host, int portNumber, int maxConnections);
void Server_start(Server *);
void Server_free(Server **);

// Signal handling
int Server_catch(int sig, void (*handler)(int));
void Server_stop(int signal);

// Send and receive data from client sockets
int Server_send(SOCKET client, const char* message, size_t length);
int Server_receive(SOCKET client, char *buffer, size_t length);

// Manages a client's thread
void* Server_handleClient(void* data);

// Broadcast messages to all connected clients
void* Server_handleBroadcast(void* data);
void Server_broadcast(char *message, size_t length);

// Closes a client connection and related thread
void Server_dropClient(SOCKET client);

// Find a Client object in the list given a nickname or a thread id
Client *Server_getClientInfoForThread(pthread_t clientThreadID);
Client *Server_getClientInfoForNickname(char *clientNickname);

// Comparison functions for List_search
int Client_findByThreadID(const ListData *a, const ListData *b, size_t size);
int Client_findByNickname(const ListData *a, const ListData *b, size_t size);

// Authenticate a client connection using a nickname
bool Server_authenticate(SOCKET client);

/**
 * Creates and initialises the server object
 * @param[in] host The listening IP address
 * @param[in] portNumber The listening TCP port
 * @param[in] maxConnections The maximum number of client connetions
 */
Server *Server_init(const char *host, int portNumber, int maxConnections) {
  if (server != NULL) {
    Fatal("The server process is already running");
  }

  // Hold socket settings
  struct addrinfo options;
  memset(&options, 0, sizeof(options));

  // Hold the return value for getaddrinfo()
  struct addrinfo *bindAddress;

  // Hold the server port number as string
  char serverPort[6] = {0};
  sprintf(serverPort, "%d", portNumber);

  // AF_INET = accepts only IPv4 host values
  // AF_INET6 = accepts only IPv6 host values
  // PF_UNSPEC = accepts any IP types depending on system configuration
  options.ai_family = PF_UNSPEC;
  options.ai_socktype = SOCK_STREAM; // TCP connections
  // AI_PASSIVE = socket used for listening
  // AI_ADDRCONFIG = if receives IPv4 returns IPv4, if receives IPv6 returns IPv6
  options.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;

  // Returns a binary IP address representation for bind()
  if (getaddrinfo(host, serverPort, &options, &bindAddress)) {
    Fatal("Invalid IP/port configuration: %s", gai_strerror(SOCKET_getErrorNumber()));
  }

  server = (Server *)calloc(sizeof(Server), 1);

  server->socket = Socket_new(bindAddress->ai_family, bindAddress->ai_socktype, bindAddress->ai_protocol);
  Socket_unsetIPV6Only(server->socket);
  Socket_setReusableAddress(server->socket);

  Socket_bind(server->socket, bindAddress->ai_addr, bindAddress->ai_addrlen);

  // Note: if the bind fails here (eg server is already running),
  // the memory allocated for the server and bindAddress vars will not be freed

  // We don't need bindAddress anymore
  freeaddrinfo(bindAddress);

  Socket_listen(server->socket, maxConnections);
  server->host = strdup(host);
  server->port = portNumber;
  server->maxConnections = maxConnections;

  return server;
}

/**
 * Frees the memory allocated for the server object
 * @param[in] this Double pointer to a server structure
 */
void Server_free(Server **this) {
  if (this != NULL) {
    free((*this)->host);
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
   struct sigaction action;
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);

   // We also want to unset the SA_RESTART flag for the handler(s),
   // which would make accept() return -1 on the signal's reception.
   action.sa_flags = 0;

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

  while (!terminate) {

    // Initialise a temporary client variable
    Client client;
    memset(&client, 0, sizeof(Client));
    client.length = sizeof(client.address);
    client.socket = accept(this->socket, (struct sockaddr*) &(client).address, &(client).length);
    if (!SOCKET_isValid(client.socket)) {
      if (EINTR == SOCKET_getErrorNumber()) {
        Info("%s", strerror(SOCKET_getErrorNumber()));
      } else {
        Error("accept() failed (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      }
      continue;
    }

    // A client has connected, log the client info
    getnameinfo(
      (struct sockaddr*)&(client).address,
      client.length, client.host, kMaxClientHostLength,
      0, 0,
      NI_NUMERICHOST
    );
    Info("New connection from %s", client.host);

    pthread_t clientThreadID = 0;
    if (clients->length < kMaxClientConnections) {

      // Add client to the list (the client is cloned)
      List_append(clients, &client, sizeof(Client));

      // Get pointer to a copy of the inserted client,
      // because the temporary client var will be overridden
      // at the beginning of the next cycle
      Client *last = (Client *)List_last(clients);

      // Start client thread
      pthread_create(&clientThreadID, NULL, Server_handleClient, &(last->socket));
      last->threadID = clientThreadID; // Update client list item
      pthread_detach(clientThreadID);
    } else {
      Info("Connection limits reached");
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
  Server_free(&server);
}

/**
 * Sends data to a socket using a loop to ensure all data is sent
 * @param[in] client The client socket
 * @param[in] message The message to send
 * @param[in] length The length of the message to send
 */
int Server_send(SOCKET client, const char* message, size_t length) {
  size_t sentTotal = 0;
  char *data = (char*)message; // points to the beginning of the message
  do {
    int sent = send(client, data, length - sentTotal, 0);
    if (sent < 0) {
      Error(
        "send() failed: (%d): %s",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      break;
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
  // The (0*size) statement avoids the 'unused parameter' error at compile time
  pthread_t threadB = *((pthread_t *)b) + (0 * size);
  return client->threadID - threadB;
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
  // The (0*size) statement avoids the 'unused parameter' error at compile time
  return strncmp(client->nickname, nickname, kMaxNicknameLength + (0 * size));
}

/**
 * Removes a client object from the list of connected clients
 * and closes the connection
 * @param[in] client The client's socket to close
 */
void Server_dropClient(SOCKET client) {
  pthread_t clientThreadID = pthread_self();

  // Close client socket
  SOCKET_close(client);

  // Drop client from the clients list
  // We need to pass sizeof(Client) as size or the item will not be compared
  int index = List_search(clients, &clientThreadID, sizeof(Client), Client_findByThreadID);
  if (index >= 0) {
    if (!List_delete(clients, index)) {
      Warn("Unable to drop client %d with thread ID %lu", index, clientThreadID);
    }
  } else {
    Warn("Unable to find client with thread ID %lu", clientThreadID);
  }
  Info("Closing client thread %lu", clientThreadID);
  pthread_exit(NULL);
}

/**
 * Lookup a Client in the list by its thread ID
 * @param[in] clientThreadID Thread ID to lookup
 * @param[out] A pointer to a Client structure or NULL
 */
Client *Server_getClientInfoForThread(pthread_t clientThreadID) {
  int index = List_search(clients, &clientThreadID, sizeof(Client), Client_findByThreadID);
  if (index >= 0) {
    return (Client *)List_item(clients, index);
  }
  return NULL;
}

/**
 * Lookup a Client in the list by its nickname
 * @param[in] clientNickname Nickname to lookup
 * @param[out] A pointer to a Client structure or NULL
 */
Client *Server_getClientInfoForNickname(char *clientNickname) {
  int index = List_search(clients, clientNickname, sizeof(Client), Client_findByNickname);
  if (index >= 0) {
    return (Client *)List_item(clients, index);
  }
  return NULL;
}

/**
 * Receives a message from a connected client until
 * a null terminator is found or the buffer is full
 * @param[in] client Socket to receive from
 * @param[in] buffer Char buffer to store the received data
 * @param[in] length Length of the char buffer
 * @param[out] The number of bytes received
 */
int Server_receive(SOCKET client, char *buffer, size_t length) {
  char *data = buffer; // points at the start of buffer
  size_t total = 0;
  do {
    int bytesReceived = recv(client, buffer, length - total, 0);

    // The remote client closed the connection
    if (bytesReceived == 0) return 0;

    // There has been an error somewhere
    if (bytesReceived < 0) return bytesReceived;

    data += bytesReceived;
    total += bytesReceived;
  } while(*data != 0 && total < (length - 1));

  // Adding safe terminator in case of loop break
  if (*data != 0) *(data + 1) = 0;

  return total;
}

/**
 * Authenticates a client connection
 * Currently it only ensures that another client is not already connected
 * using the same nickname, and the client entry is in the clients list
 * @param[in] client Socket to communicate with
 * @param[out] Success or failure
 */
bool Server_authenticate(SOCKET client) {
  char request[kBufferSize] = {0};
  char response[kBufferSize] = {0};

  Message_format(kMessageTypeNick, request, kBufferSize, "Please enter a nickname:");
  Server_send(client, request, strlen(request));

  int received = Server_receive(client, response, kBufferSize);
  if (received > 0) {
    if (kMessageTypeNick == Message_getType(response)) {
      // The client has sent a nick in the format '/nick Name'
      // In order to have a full 20 chars nickname, we need to add a 7 chars pad
      // to the length: 5chars for the /nick prefix, + 1 space + null-terminator
      char *nick = Message_getContent(response, kMessageTypeNick, kMaxNicknameLength + 7);
      Client *clientInfo = NULL;
      // Lookup if a client is already logged with the provided nickname
      clientInfo = Server_getClientInfoForNickname(nick);
      if (clientInfo == NULL) {
        // The user's nickname is unique
        // Lookup client by thread
        clientInfo = Server_getClientInfoForThread(pthread_self());
        if (clientInfo != NULL) {
          // Update client entry
          snprintf(clientInfo->nickname, kMaxNicknameLength, "%s", nick);
          Info("User %s (%d) authenticated successfully!", clientInfo->nickname, strlen(clientInfo->nickname));
          Message_free(&nick);
          return true;
        }
        Error("Authentication: client info not found for client %lu", pthread_self());
      }
      Info("Client with nick '%s' is already logged in", nick);
      Message_free(&nick);
    }
  }
  // We leave error management or client disconnection to the calling function
  return false;
}


/**
 * Handles communications with a single client, it is spawned
 * on a new thread for every connected client
 * @param[in] socket Pointer to a client socket
 */
void* Server_handleClient(void* socket) {
  pthread_t me = pthread_self();
  SOCKET *client = (SOCKET *)socket;
  char messageBuffer[kBufferSize] = {0};

  Info("Starting new client thread %lu", me);

  // Send a welcome message
  memset(messageBuffer, '\0', kBufferSize);
  Message_format(kMessageTypeOk, messageBuffer, kBufferSize, "Welcome to C2hat!");
  Server_send(*client, messageBuffer, strlen(messageBuffer));

  // Ask for a nickname
  if (!Server_authenticate(*client)) {
    Info("Authentication failed for client thread %lu", me);
    memset(messageBuffer, '\0', kBufferSize);
    Message_format(kMessageTypeErr, messageBuffer, kBufferSize, "Authentication failed");
    Server_send(*client, messageBuffer, strlen(messageBuffer));
    Server_dropClient(*client);
  }

  // Say Hello to the new user
  Client *clientInfo = Server_getClientInfoForThread(me);
  if (clientInfo == NULL) {
    Error("Client info not found for client %lu", me);
    Server_dropClient(*client);
  }
  memset(messageBuffer, '\0', kBufferSize);
  Message_format(kMessageTypeOk, messageBuffer, kBufferSize, "Hello %s!", clientInfo->nickname);
  Server_send(*client, messageBuffer, strlen(messageBuffer));

  // Broadcast that a new client has joined
  memset(messageBuffer, '\0', kBufferSize);
  Message_format(kMessageTypeLog, messageBuffer, kBufferSize, "%s just joined the chat", clientInfo->nickname);
  Server_broadcast(messageBuffer, strlen(messageBuffer));

  // Start the chat
  while(!terminate) {
    // Initialise buffer for client data
    memset(messageBuffer, '\0', kBufferSize);

    // Listen for data
    int received = Server_receive(*client, messageBuffer, kBufferSize);
    if (received < 0) {
      if (SOCKET_getErrorNumber() == 0) {
        Info("Connection closed by remote client %d", ECONNRESET);
      } else {
        Error("recv() failed: (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      }
      break;
    }

    if (received == 0) {
      Info("Connection closed by remote client %d", ECONNRESET);
      break;
    }

    if (received > 0) {
      Info("Received: %.*s", received, messageBuffer);

      int messageType = Message_getType(messageBuffer);
      if (kMessageTypeQuit == messageType) break;

      char *messageContent = Message_getContent(messageBuffer, kMessageTypeMsg, kBufferSize);
      char broadcastBuffer[kBroadcastBufferSize] = {0};
      switch (messageType) {
        case kMessageTypeMsg:
          if (messageContent != NULL && strlen(messageContent) > 0) {

            // Send /ok to the client to acknowledge the correct message
            memset(messageBuffer, '\0', kBufferSize);
            Message_format(kMessageTypeOk, messageBuffer, kBufferSize, "");
            Server_send(*client, messageBuffer, strlen(messageBuffer));

            // Broadcast the message to all clients using the format '/msg [<20charUsername>]: ...'
            Message_format(kMessageTypeMsg, broadcastBuffer, kBroadcastBufferSize, "[%s]: %s", clientInfo->nickname, messageContent);
            Server_broadcast(broadcastBuffer, strlen(broadcastBuffer));
            Message_free(&messageContent);
          }
        break;
        default:
          ; // Ignore for now...
      }
    }
  }

  // Broadcast that client has left
  memset(messageBuffer, '\0', kBufferSize);
  Message_format(kMessageTypeLog, messageBuffer, kBufferSize, "%s just left the chat", clientInfo->nickname);
  Server_broadcast(messageBuffer, strlen(messageBuffer));

  // Close the connection
  Server_dropClient(*client);

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
  while (!terminate) {
    do {
      // Lock Message Queue
      item = Queue_dequeue(messages);
      // Unlock queue
      if (item == NULL) break; // Queue is empty

      // Lock clients
      Client *client;
      List_rewind(clients);
      while ((client = (Client *)List_next(clients)) != NULL) {
        int sent = Server_send(client->socket, (char*)item->content, item->length);
        if (sent <= 0) Server_dropClient(client->socket);
      }
      QueueData_free(&item);
      // Unlock clients
    } while(!Queue_empty(messages));
  }
  Info("Closing broadcast thread %lu", me);
  pthread_exit(data);
  return NULL;
}

/**
 * Adds a message to the broadcast queue
 * @param[in] message Message to enqueue
 * @param[in] length Size of the data to enqueue
 */
void Server_broadcast(char *message, size_t length) {
  // Lock queue
  Queue_enqueue(messages, message, length);
  // Unlock queue
}
