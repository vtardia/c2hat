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

    // Tells Visual Studio to link the declare library on compile
    // if using MingW on Windowz we need to manually use '-lws2_32'
    #pragma comment(lib, "ws2_32.lib")

    #define SOCKET_isValid(s) ((s) != INVALID_SOCKET)
    #define SOCKET_close(s) closesocket(s)
    #define SOCKET_getErrorNumber() (WSAGetLastError())
  #else
    #include <sys/types.h>
    #include <sys/socket.h>
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

#endif
