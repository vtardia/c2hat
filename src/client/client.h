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

#ifndef CLIENT_H
#define CLIENT_H
  #include <stdio.h>
  #include <stdbool.h>
  #include "socket/socket.h"
  #include "message/message.h"
  #include "../c2hat.h"

  enum {
    /// Can hold 'localhost' or an IPv4 or an IPv6 string
    kMaxHostnameSize = 128,
    /// Max character length for the server port
    kMaxPortSize = 6
  };

  /// Contains the client startup parameters
  typedef struct {
    char user[kMaxNicknameSize];
    char host[kMaxHostnameSize];
    char port[kMaxPortSize];
    char caCertFilePath[kMaxPath];
    char caCertDirPath[kMaxPath];
    char logDirPath[kMaxPath];
    unsigned int logLevel;
  } ClientOptions;

  // Opaque structure that contains client connection details
  typedef struct _C2HatClient C2HatClient;

  // Creates a connected network client object
  C2HatClient *Client_create(ClientOptions *options);

  // Tries to connect a client to the network
  bool Client_connect(C2HatClient *this, const char *host, const char *port);

  // Authenticates with the C2Hat server
  bool Client_authenticate(C2HatClient *this, const char *username);

  // Returns the raw socket for the given client
  SOCKET Client_getSocket(const C2HatClient *client);

  // Returns the message buffer for the given client
  void *Client_getBuffer(C2HatClient *this);

  // Sends data through the client's socket
  int Client_send(const C2HatClient *client, const C2HMessage *message);

  // Receives data through the client's socket
  int Client_receive(C2HatClient *client);

  // Safely disconnects a client instance
  void Client_disconnect(C2HatClient *this);

  // Destroys a network client
  void Client_destroy(C2HatClient **client);
#endif
