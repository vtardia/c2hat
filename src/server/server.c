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

// Removes a client object from the list of connected clients
void Server_dropClient(pthread_t clientThreadID) {
  // We need to pass sizeof(Client) as size or the item will not be compared
  int index = List_search(clients, &clientThreadID, sizeof(Client), Client_findByThreadID);
  if (index >= 0) {
    if (!List_delete(clients, index)) {
      Warn("Unable to drop client %d with thread ID %lu", index, clientThreadID);
    }
    return;
  }
  Warn("Unable to find client with thread ID %lu", clientThreadID);
}

// Main Client thread, handle communications with a single client
void* Server_handleClient(void* socket) {
  pthread_t me = pthread_self();
  SOCKET *client = (SOCKET *)socket;

  char clientMessage[kBufferSize];

  char *welcomeMessage = "/ok Welcome to C2hat!";
  Server_send(*client, welcomeMessage, strlen(welcomeMessage));

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
  SOCKET_close(*client);
  // Drop client from the clients list
  Server_dropClient(me);
  Info("Closing client thread %lu", me);
  pthread_exit(NULL);
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
