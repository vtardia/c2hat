/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef CLIENT_H
#define CLIENT_H
  #include <stdio.h>
  #include <stdbool.h>
  #include "socket/socket.h"
  #include "../c2hat.h"

  enum {
    kMaxHostnameSize = 128,
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
  int Client_send(const C2HatClient *client, const char *buffer, size_t length);

  // Receives data through the client's socket
  int Client_receive(C2HatClient *client);

  // Safely disconnects a client instance
  void Client_disconnect(C2HatClient *this);

  // Destroys a network client
  void Client_destroy(C2HatClient **client);
#endif
