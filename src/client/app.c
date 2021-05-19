/*
 * Copyright (C) 2021 Vito Tardia
 */

#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#include "app.h"
#include "ui.h"
#include "message/message.h"

/// Loop termination flag
static bool terminate = false;

/// Initialise sockets (win only)
void App_init() {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    exit(EXIT_FAILURE);
  }
#endif
}

/// Cleanup sockets (win only) and return
int App_cleanup(int result) {
#if defined(_WIN32)
  WSACleanup();
#endif
  return result;
}

/// Sets the termination flag on SIGINT or SIGTERM
void App_terminate(int signal) {
  // Disable unused parameter warning
  (void)(signal);
  terminate = true;
}

/// Catches interrupt signals
int App_catch(int sig, void (*handler)(int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   return sigaction (sig, &action, NULL);
}

/// Listens for data from the server and updates the chat log window
void *App_listen(void *client) {
  C2HatClient *this = (C2HatClient *) client;
  SOCKET server = Client_getSocket(this);

  char buffer[kBufferSize] = {0};
  int received = 0;

  fd_set reads;
  FD_ZERO(&reads);
  FD_SET(server, &reads);

  struct timespec ts;
  int msec = 200;
  ts.tv_sec = msec / 1000;
  ts.tv_nsec = (msec % 1000) * 1000000;

  while(!terminate) {
    if (!SOCKET_isValid(server)) break;
    if (select(server + 1, &reads, 0, 0, NULL) < 0) {
      if (SOCKET_getErrorNumber() == EINTR) break; // Signal received before timeout
      fprintf(stderr, "select() failed. (%d): %s\n", SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      terminate = true;
      break;
    }
    if (FD_ISSET(server, &reads)) {
      memset(buffer, 0, kBufferSize);
      received = Client_receive(this, buffer, kBufferSize);
      if (received <= 0) {
        terminate = true;
        break;
      }
      UILogMessage(buffer, received);
    }
    // Sleep for a bit,
    // enough for the termination signal to be recognised
    nanosleep(&ts, NULL);
  }
  FD_CLR(server, &reads);
  SOCKET_close(server);
  pthread_exit(NULL);
}

/// Implements the application input loop
void App_run(C2HatClient *this) {
  while(!terminate) {
    // Reset the UI input facility
    UILoopInit();

    // Request user input
    char buffer[kBufferSize] = {0};
    int inputSize = UIGetUserInput(buffer, kBufferSize);
    // User pressed F1 or other exit commands
    if (inputSize < 0) {
      terminate = true;
      break;
    }

    int messageType = Message_getType(buffer);
    if (messageType == kMessageTypeQuit) break;

    // If the input is not a command, wrap it into a message payload
    char message[kBufferSize] = {0};
    if (!messageType) {
      Message_format(kMessageTypeMsg, message, kBufferSize, "%s", buffer);
    } else {
      // Send the message as is
      memcpy(message, buffer, inputSize);
    }
    int sent = Client_send(this, message, strlen(message) + 1);

    // If the connection drops, break and close
    if (sent < 0) break;
  }

  // Try a clean clean exit
  Client_send(this, "/quit", strlen("/quit") + 1);
}