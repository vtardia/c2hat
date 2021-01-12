#include "server.h"

static bool terminate = false;

SOCKET Server_new(const char *host, int portNumber, int maxConnections) {
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

  SOCKET server = Socket_new(bindAddress->ai_family, bindAddress->ai_socktype, bindAddress->ai_protocol);
  Socket_unsetIPV6Only(server);
  Socket_setReusableAddress(server);

  Socket_bind(server, bindAddress->ai_addr, bindAddress->ai_addrlen);

  // We don't need bindAddress anymore
  freeaddrinfo(bindAddress);

  Socket_listen(server, maxConnections);

  return server;
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
void Server_start(SOCKET server) {

  // Stop from Ctrl+C
  Server_catch(SIGINT, Server_stop);

  // Stop from System or control process
  Server_catch(SIGTERM, Server_stop);

  // Ignoring SIGPIPE (= sending data to a closed socket)
  Server_catch(SIGPIPE, SIG_IGN);

  while (!terminate) {

    // Wait for a client to connect
    struct sockaddr_storage clientAddress;
    socklen_t clientLen = sizeof(clientAddress);
    SOCKET client = accept(server, (struct sockaddr*) &clientAddress, &clientLen);
    if (!SOCKET_isValid(client)) {
      Error("accept() failed (%d): %s", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      continue;
    }

    // A client has connected, log the client info
    char addressBuffer[100];
    getnameinfo(
      (struct sockaddr*)&clientAddress,
      clientLen, addressBuffer, sizeof(addressBuffer),
      0, 0,
      NI_NUMERICHOST
    );
    Info("New connection from %s\n", addressBuffer);

    // Placeholder code: send a message and close the connection
    char *message = "Hello! Sorry, there's nothing to see here, bye!\n";
    send(client, message, strlen(message), 0);
    SOCKET_close(client);

    // Real code: start a new thread to handle the client here
  }
  Info("Terminating...");
  SOCKET_close(server);
}
