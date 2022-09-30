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

#ifndef SOCKET_H
#define SOCKET_H

  // Winsock and POSIX have differences in how a socket type
  // is represented and in error handling.
  #if defined(_WIN32)
    #ifndef _WIN32_WINNT
      #define _WIN32_WINNT 0x0600
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <sys/sioctl.h>

    // Tells Visual Studio to link the declare library on compile
    // if using MingW on Windowz we need to manually use '-lws2_32'
    #pragma comment(lib, "ws2_32.lib")

    #define SOCKET_isValid(s) ((s) != INVALID_SOCKET)
    #define SOCKET_close(s) closesocket(s)
    #define SOCKET_getErrorNumber() (WSAGetLastError())
  #else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>

    typedef int SOCKET;
    #define SOCKET_isValid(s) ((s) >= 0)
    #define SOCKET_close(s) close(s)
    #define SOCKET_getErrorNumber() (errno)
  #endif

  #if !defined(IPV6_V6ONLY)
    #define IPV6_V6ONLY 27
  #endif


  #if !defined(AI_ALL)
    #define AI_ALL 0x0100
  #endif

  // Create a new socket with the provided options (family, type, protocol)
  SOCKET Socket_new(int domain, int type, int protocol);

  // Bind the given socket to an IP address/port combination
  void Socket_bind(SOCKET server, const struct sockaddr *addr, socklen_t addrlen);

  // Start listening on a given socket
  void Socket_listen(SOCKET server, int maxConnections);

  // Set the socket to listen to both IPv4 and IPv6 if supported by the system
  void Socket_unsetIPV6Only(SOCKET);

  // Allow the given socket to reuse the same address without waiting 20 seconds
  void Socket_setReusableAddress(SOCKET);

  // Set the given socket to be non-blocking
  void Socket_setNonBlocking(SOCKET this);

#endif
