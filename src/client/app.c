/*
 * Copyright (C) 2021 Vito Tardia
 */

#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#include "wtrim/wtrim.h"
#include "app.h"
#include "ui.h"
#include "message/message.h"
#include "logger/logger.h"

/// Loop termination flag
static bool terminate = false;

/// Keeps track of the main thread id so other threads can send signals
static pthread_t mainThreadID = 0;

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
   struct sigaction action = {};
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   return sigaction (sig, &action, NULL);
}

/// Listens for data from the server and updates the chat log window
void *App_listen(void *client) {
  C2HatClient *this = (C2HatClient *) client;
  SOCKET server = Client_getSocket(this);
  MessageBuffer *buffer = Client_getBuffer(this);

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
      // Ignore a signal received before timeout, will be managed by handler
      if (SOCKET_getErrorNumber() == EINTR) continue;

      fprintf(
        stderr, "select() failed. (%d): %s\n",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      terminate = true;
      break;
    }
    if (FD_ISSET(server, &reads)) {
      received = Client_receive(this);
      if (received <= 0) {
        terminate = true;
        break;
      }
      char *response = NULL;
      do {
        response = Message_get(buffer);
        if (!response) break;

        // Push the message to be read by the main thread
        UIPushMessage(response, strlen(response) + 1);
        Message_free(&response);
      } while(response != NULL);

      // Alert the main thread that there are messages to read
      pthread_kill(mainThreadID, SIGUSR2);
    }
    // Sleep for a bit,
    // enough for the termination signal to be recognised
    nanosleep(&ts, NULL);
  }

  // Clear file descriptors and close the socket
  FD_CLR(server, &reads);
  SOCKET_close(server); // Or Client_disconnect()?

  // Tell the main thread that it needs to close the UI
  pthread_kill(mainThreadID, SIGUSR1);

  // Clean exit
  pthread_exit(NULL);
}

/// Implements the application input loop
void App_run(C2HatClient *this) {
  // Initialise the thread id in order to receive messages
  mainThreadID = pthread_self();

  while(!terminate) {
    // Reset the UI input facility
    UILoopInit();

    // Request user input as Unicode
    wchar_t buffer[kMaxMessageLength] = {};
    size_t inputSize = UIGetUserInput(buffer, kMaxMessageLength);
    // User pressed F1 or other exit commands
    if ((int)inputSize < 0) {
      terminate = true;
      break;
    }

    // Clean input buffer
    wchar_t *trimmedBuffer = wtrim(buffer, NULL);

    // Convert it into UTF-8
    char messageBuffer[kBufferSize] = {};
    wcstombs(messageBuffer, trimmedBuffer, kBufferSize);

    Debug("App_run - user typed: %s", messageBuffer);

    int messageType = Message_getType(messageBuffer);
    if (messageType == kMessageTypeQuit) break;

    // If the input is not a command, wrap it into a message payload
    char message[kBufferSize] = {};
    if (!messageType) {
      Message_format(kMessageTypeMsg, message, kBufferSize, "%s", messageBuffer);
    } else {
      // Send the message as is
      memcpy(message, messageBuffer, inputSize);
    }
    int sent = Client_send(this, message, strlen(message) + 1);

    // If the connection drops, break and close
    if (sent < 0) break;
  }

  // Try a clean clean exit
  Client_send(this, "/quit", strlen("/quit") + 1);
}
