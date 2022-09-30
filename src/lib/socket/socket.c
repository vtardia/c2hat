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

#include "socket.h"
#include "logger/logger.h"

SOCKET Socket_new(int domain, int type, int protocol) {
  SOCKET this = socket(domain, type, protocol);
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
    Error(
      "Unable to unset IPv6 Only (%d): %s\n",
      SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
    );
  }
}

// Allow to reopen the socket immediately otherwise there is 20sec
// timeframe in which we have an "address in use error"
void Socket_setReusableAddress(SOCKET this) {
  int optReuseAddress = 1;
  if (setsockopt(this, SOL_SOCKET, SO_REUSEADDR, (const void *)&optReuseAddress , sizeof(int))) {
    Error(
      "Unable to set reusable address (%d): %s\n",
      SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
    );
  }
}

// Set the given socket to be non-blocking using ioctl()
void Socket_setNonBlocking(SOCKET this) {
  int optNonBlocking = 1;
  int rc = ioctl(this, FIONBIO, (char *)&optNonBlocking);
  if (rc < 0) {
    Error(
      "Unable to set non-blocking socket (%d): %s\n",
      SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
    );
  }
}

void Socket_bind(SOCKET this, const struct sockaddr *addr, socklen_t addrlen) {
  // bind() returns 0 on success, non-zero on failure
  if (bind(this, addr, addrlen)) {
    Fatal(
      "bind() failed. (%d): %s\n",
      SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
    );
  }
  char addressBuffer[100] = {};
  char serviceBuffer[100] = {};
  getnameinfo(
    addr, addrlen,
    addressBuffer, sizeof(addressBuffer),
    serviceBuffer, sizeof(serviceBuffer),
    NI_NUMERICHOST | NI_NUMERICSERV
  );
  Info("Bind done on %s:%s", addressBuffer, serviceBuffer);
}

void Socket_listen(SOCKET this, int maxConnections) {
  // listen() returns a negative value on error
  if (listen(this, maxConnections) < 0) {
    Fatal(
      "Unable to listen for connections (%d): %s\n",
      SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
    );
  }
  Info("Waiting for incoming connections...");
}
