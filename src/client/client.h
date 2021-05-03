/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef CLIENT_H
#define CLIENT_H
  #include <stdio.h>
  #include <stdbool.h>

  // Opaque structure that contains client connection details
  typedef struct _C2HatClient C2HatClient;

  // Creates a connected network client object
  C2HatClient *Client_create();

  // Tries to connect a client to the network
  bool Client_connect(C2HatClient *this, const char *host, const char *port);

  // Authenticates with the C2Hat server
  bool Client_authenticate(C2HatClient *this, const char *username);

  // Destroys a network client
  void Client_destroy(C2HatClient **client);

  // Runs theclient application's infinite loop
  void Client_run(C2HatClient *client, FILE *in, FILE *out, FILE *err);

  // Sends data through the client's socket
  int Client_send(const C2HatClient *client, const char *buffer, size_t length);

  // Receives data through the client's socket
  int Client_receive(const C2HatClient *client, char *buffer, size_t length);
#endif
