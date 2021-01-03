#include "socket.h"

SOCKET Socket_new(struct addrinfo *address) {
  SOCKET this = socket(
    address->ai_family,
    address->ai_socktype,
    address->ai_protocol
  );
  if (!SOCKET_isValid(this)) {
    Fatal("socket() failed. (%d)\n", SOCKET_getErrorNumber());
  }
  Info("Socket created");
  return this;
}

// Enable support for both IPv4 and IPv6, if the operating system allows it
void Socket_unsetIPV6Only(SOCKET this) {
  int optIPV6Only = 0;
  // Clear the IPV6_ONLY option, returns 0 on success
  if (setsockopt(this, IPPROTO_IPV6, IPV6_V6ONLY, (void*)&optIPV6Only, sizeof(int))) {
    Error("Unable to unset IPv6 Only (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
  }
}

// Allow to reopen the socket immediately otherwise there is 20sec
// timeframe in which we have an "address in use error"
void Socket_setReusableAddress(SOCKET this) {
  int optReuseAddress = 1;
  if (setsockopt(this, SOL_SOCKET, SO_REUSEADDR, (const void *)&optReuseAddress , sizeof(int))) {
    Error("Unable to set reusable address (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
  }
}

void Socket_bind(SOCKET this, struct addrinfo *bindAddress) {
  // bind() returns 0 on success, non-zero on failure
  if (bind(this, bindAddress->ai_addr, bindAddress->ai_addrlen)) {
    Fatal("bind() failed. (%d)\n", SOCKET_getErrorNumber());
  }
  char addressBuffer[100];
  char serviceBuffer[100];
  getnameinfo(
    bindAddress->ai_addr, bindAddress->ai_addrlen,
    addressBuffer, sizeof(addressBuffer),
    serviceBuffer, sizeof(serviceBuffer),
    NI_NUMERICHOST | NI_NUMERICSERV
  );
  Info("Bind done on %s:%s", addressBuffer, serviceBuffer);
}

void Socket_listen(SOCKET this, int maxConnections) {
  // listen() returns a negative value on error
  if (listen(this, maxConnections) < 0) {
    Fatal("Unable to listen for connections");
  }
  Info("Waiting for incoming connections...");
}
