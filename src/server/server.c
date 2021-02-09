#include "server.h"

#include <pthread.h>

enum {
  kMaxClientConnections = 5,
  kMaxClientHostLength = NI_MAXHOST,
  kMaxNicknameLength = 20,
  kBufferSize = 1024 // Includes NULL term
};

// Holds data for queued messages
typedef struct {
  char sender[kMaxNicknameLength + 1];
  char content[kBufferSize];
  int  contentLength;
} Message;

// Hold details for connected clients
typedef struct {
  pthread_t threadID;
  char nickname[kMaxNicknameLength + 1]; // Client name + NULL terminator
  SOCKET socket;
  struct sockaddr_storage address; // Binary IP address
  socklen_t length;
  char host[kMaxClientHostLength]; // Pretty string IP address
} Client;

// This is the singleton instance for our server
struct Server {
  char *host;
  int port;
  int maxConnections;
  SOCKET socket;
};
static Server *server = NULL;

static bool terminate = false;

static List *clients = NULL;
// static Queue *messages = NULL;

void* Server_handleClient(void* data);
void* Server_handleBroadcast(void* data);
// void Server_broadcast(void* data);
void Server_send(SOCKET client, const char* message, size_t length);

// Initialise the server object
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

  // We don't need bindAddress anymore
  freeaddrinfo(bindAddress);

  Socket_listen(server->socket, maxConnections);
  server->host = strdup(host);
  server->port = portNumber;
  server->maxConnections = maxConnections;

  return server;
}

// Free memory for the server object
void Server_free(Server **this) {
  if (this != NULL) {
    free((*this)->host);
    memset(*this, 0, sizeof(Server));
    free(*this);
    *this = NULL;
  }
}

// Set the server termination flag
void Server_stop(int signal) {
  char *sig_name = strsignal(signal);
  Info("Signal '%s' received, shutting down...", sig_name);
  terminate = true;
}

// Catch interrupt signals
int Server_catch(int sig, void (*handler)(int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);

   // We also want to unset the SA_RESTART flag for the handler(s),
   // which would make accept() return -1 on the signal's reception.
   action.sa_flags = 0;

   return sigaction (sig, &action, NULL);
}

// Start a server on the given socket
void Server_start(Server *this) {

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

  // messages = Queue_new();
  // if (messages == NULL) {
  //   Fatal("Unable to initialise message queue");
  // }

  while (!terminate) {

    // Initialise a temporary client variable
    Client client;
    memset(&client, 0, sizeof(Client));
    client.length = sizeof(client.address);
    client.socket = accept(this->socket, (struct sockaddr*) &(client).address, &(client).length);
    if (!SOCKET_isValid(client.socket)) {
      Error("accept() failed (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
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
  // Queue_free(&messages);

  // Cleanup socket and server
  SOCKET_close(this->socket);
  Server_free(&server);
}

// Send data to a socket ensuring all data is sent
void Server_send(SOCKET client, const char* message, size_t length) {
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
}

// Comparison function to lookup a client by its ThreadID
// A is a Client object, b is a thread ID
// The (0*size) statement is there to avoid errors for unused parameter
int Client_findByThreadID(const ListData *a, const ListData *b, size_t size) {
  Client *client = (Client *)a;
  pthread_t threadB = *((pthread_t *)b) + (0 * size);
  return client->threadID - threadB;
}

// Comparison function to lookup a client by its Nickname
// A is a Client object, b is a char pointer
// The (0*size) statement is there to avoid errors for unused parameter
int Client_findByNickname(const ListData *a, const ListData *b, size_t size) {
  Client *client = (Client *)a;
  char *nickname = (char *)b;
  return strncmp(client->nickname, nickname, kMaxNicknameLength + (0 * size));
}

// Removes a client object from the list of connected clients
// and close the connection
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

// Lookup a Client in the list by its thread ID
Client *Server_getClientInfoForThread(pthread_t clientThreadID) {
  int index = List_search(clients, &clientThreadID, sizeof(Client), Client_findByThreadID);
  if (index >= 0) {
    return (Client *)List_item(clients, index);
  }
  return NULL;
}

// Lookup a Client in the list by its nickname
Client *Server_getClientInfoForNickname(char *clientNickname) {
  int index = List_search(clients, clientNickname, sizeof(Client), Client_findByNickname);
  if (index >= 0) {
    return (Client *)List_item(clients, index);
  }
  return NULL;
}

// Receive a message from the Client until a null terminator is found
// or the buffer is full
int Server_receive(SOCKET client, char *buffer, size_t length) {
  char *data = buffer; // points at the start of buffer
  size_t total = 0;
  do {
    int bytesReceived = recv(client, buffer, length - total, 0);
    if (bytesReceived == 0) {
      printf("Connection closed by remote client\n");
      break; // exit the whole loop
    }
    if (bytesReceived < 0) {
      fprintf(stderr, "recv() failed. (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
      break; // exit the whole loop
    }
    data += bytesReceived;
    total += bytesReceived;
  } while(*data != 0 && total < (length - 1));

  // Adding safe terminator in case of loop break
  if (*data != 0) *(data + 1) = 0;

  return total;
}

// Authenticate a client connection
// Currently it only ensures that another client is not already connected
// using the same nickname, and the client entry is in the clients list
bool Server_authenticate(SOCKET client) {
  char request[kBufferSize] = {0};
  char response[kBufferSize] = {0};

  snprintf(request, kBufferSize, "/nick Please enter a nickname:");
  Server_send(client, request, strlen(request));

  int received = Server_receive(client, response, kBufferSize);
  if (received > 0) {
    if (strncmp(response, "/nick", 5) == 0) {
      // The client has sent a nick
      char *nick = response + 6; // Nickname starts after the tag
      *(nick + kMaxNicknameLength) = 0; // Truncating nickname to its max
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
          return true;
        }
        Error("Authentication: client info not found for client %lu", pthread_self());
      }
      Info("Client with nick '%s' is already logged in", nick);
    }
  }
  return false;
}

// Main Client thread, handle communications with a single client
void* Server_handleClient(void* socket) {
  pthread_t me = pthread_self();
  SOCKET *client = (SOCKET *)socket;
  char clientMessage[kBufferSize] = {0};

  Info("Starting new client thread %lu", me);

  // Send a welcome message
  char *welcomeMessage = "/ok Welcome to C2hat!";
  Server_send(*client, welcomeMessage, strlen(welcomeMessage));

  // Ask for a nickname
  if (!Server_authenticate(*client)) {
    Info("Authentication failed for client thread %lu", me);
    char *errorMessage = "/err Authentication failed";
    Server_send(*client, errorMessage, strlen(errorMessage));
    Server_dropClient(*client);
  }

  // Say Hello to the new user
  Client *clientInfo = Server_getClientInfoForThread(me);
  if (clientInfo == NULL) {
    Error("Client info not found for client %lu", me);
    Server_dropClient(me);
  }
  char greetings[kBufferSize] = {0};
  snprintf(greetings, kBufferSize, "Hello %s!", clientInfo->nickname);
  Server_send(*client, greetings, strlen(greetings));

  // TODO: broadcast that a new client has joined

  // Start the chat
  while(!terminate) {
    // Initialise buffer for client data
    memset(clientMessage, '\0', kBufferSize);

    // Listen for data
    int received = recv(*client, clientMessage, kBufferSize, 0);
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
      Info("Received: %.*s", received, clientMessage);

      // Sending back data to the client
      Server_send(*client, clientMessage, received);
    }
  }
  Server_dropClient(*client);
  return NULL;
}

void* Server_handleBroadcast(void* data) {
  pthread_t me = pthread_self();
  while (!terminate) {
    // Info("Broadcasting messages %s...", foo);
    sleep(5);
    /*
    do {
      // Lock Message Queue
      message = Queue_pop(messages);
      // Unlock queue

      // Lock clients
      Client *client;
      while ((client = (Client *)List_next(clients)) != NULL) {
        if !Server_send(client, message) Clients_drop(client->threadID);
      }
      // Unlock clients
    } while(message != NULL);
    */
  }
  Info("Closing broadcast thread %lu", me);
  pthread_exit(data);
  return NULL;
}

// void Server_broadcast(void* data) {
//   // Lock queue
//   Queue_enqueue(messages, data);
//   // Unlock queue
// }
