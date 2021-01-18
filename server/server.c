#include "server.h"

#include <pthread.h>

typedef struct {
  struct sockaddr_storage address;
  socklen_t length;
  SOCKET socket;
} Client;

struct Server {
  char *host;
  int port;
  int maxConnections;
  SOCKET socket;
};

// This is the singleton instance for our server
static Server *server = NULL;

static bool terminate = false;

void* Server_handleClient(void* data);

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

  while (!terminate) {

    // Wait for a client to connect
    Client client;
    client.length = sizeof(client.address);
    client.socket = accept(this->socket, (struct sockaddr*) &(client).address, &(client).length);
    if (!SOCKET_isValid(client.socket)) {
      Error("accept() failed (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      continue;
    }

    // Start client thread
    pthread_t clientThreadID = 0;
    pthread_create(&clientThreadID, NULL, Server_handleClient, &client);
    pthread_detach(clientThreadID);
  }
  Info("Terminating...");
  SOCKET_close(this->socket);
  Server_free(&server);
}

void* Server_handleClient(void* data) {
  Client *client = (Client *)data;
  Server_catch(SIGTERM, Server_stop);
  pthread_detach(pthread_self());

  const int kBufferSize = 1024;
  char clientMessage[kBufferSize];

  // A client has connected, log the client info
  // TODO: encapsulate it within a function Client_getIPAddress(Client *)
  char addressBuffer[100] = {0};
  getnameinfo(
    (struct sockaddr*)&(client->address),
    client->length, addressBuffer, sizeof(addressBuffer),
    0, 0,
    NI_NUMERICHOST
  );
  Info("New connection from %s\n", addressBuffer);

  while(!terminate) {
    // Initialise buffer for client data
    memset(clientMessage, '\0', kBufferSize);

    // Listen for data
    int received = recv(client->socket, clientMessage, kBufferSize, 0);
    if (received < 0) {
      if (SOCKET_getErrorNumber() == 0) {
        Info("Connection closed by remote client %d", ECONNRESET);
      } else {
        Error("recv() failed: (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      }
      break;
    }

    if (received > 0) {
      Info("Received: %.*s", received, clientMessage);

      // Sending back data to the client
      int sentTotal = 0;
      char *data = clientMessage;
      do {
        int sent = send(client->socket, data, received, 0);
        if (sent < 0) {
          Error("send() failed: (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
          break;
        }
        data += sent;
        sentTotal += sent;
      } while (sentTotal < received);
    }
  }
  SOCKET_close(client->socket);
  Info("Closing thread %d", pthread_self());
  pthread_exit(NULL);
  return NULL;
}
