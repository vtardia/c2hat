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

#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <execinfo.h>

#include "wtrim/wtrim.h"
#include "app.h"
#include "ui.h"
#include "message/message.h"
#include "logger/logger.h"
#include "cqueue/cqueue.h"

/// Loop termination flag
static atomic_bool terminate = false;

/// Keeps track of the main thread id so other threads can send signals
static pthread_t mainThreadID = 0;

/// Keeps track of the listening thread id so other threads can send signals
static pthread_t listeningThreadID = 0;

/// Reference to a C2Hat Client object
static C2HatClient *client = NULL;

/// Contains a copy of the chat client settings
static ClientOptions settings = {};

/// Concurrent FIFO queue of chat messages
static CQueue *messages = NULL;


/// Cleanup resources and exit
void App_cleanup() {
#if defined(_WIN32)
  WSACleanup();
#endif
  if (!isendwin()) UIClean();

  if (client != NULL) {
    Debug("Cleaning up client...");
    Client_destroy(&client);
    Debug("Cleaning up client success");
  }

  if (messages != NULL) CQueue_free(&messages);
}

/// Sets the termination flag on SIGINT or SIGTERM
void App_handleSignal(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    terminate = true;
    UITerminate();
  } else if (signal == SIGUSR2) {
    UIUpdateChatLog();
  } else if (signal == SIGWINCH) {
    UIResize();
  } else if (signal == SIGSEGV) {
    // Should prevent terminal messup on crash
    if (!isendwin()) endwin();

    if (settings.logLevel <= LOG_DEBUG) {
      void *trace[20] = {};
      size_t size;
      size = backtrace(trace, sizeof(trace));
      char *error = "❌ Segmentation fault happened, backtrace below:\n";
      write(STDERR_FILENO, error, strlen(error));
      backtrace_symbols_fd(trace, size, STDERR_FILENO);
    } else {
      char *error = "❌ Segmentation fault happened, enable debug to see the stacktrace\n";
      write(STDERR_FILENO, error, strlen(error));
    }
    exit(EXIT_FAILURE);
  } else {
    Info("Unhandled signal received %s", strerror(errno));
  }
}

/// Catches interrupt signals
int App_catch(int sig, void (*handler)(int)) {
   struct sigaction action = {};
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   return sigaction (sig, &action, NULL);
}

/// Initialise the application resources
void App_init(ClientOptions *options) {
  if (client != NULL) return; // Already initialised

  // Register shutdown function
  atexit(App_cleanup);

  // Copy the settings for later use
  memcpy(&settings, options, sizeof(ClientOptions));

  App_catch(SIGSEGV, App_handleSignal);

// Initialise sockets on Windows
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize\n");
    exit(EXIT_FAILURE);
  }
#endif

  // Create chat client...
  client = Client_create(&settings);
  if (client == NULL) {
    fprintf(stderr, "Chat client creation failed\n");
    exit(EXIT_FAILURE);
  }

  // ...and try to connect
  // NOTE: from now on, stderr may be redirected to the log facility
  if (!Client_connect(client, settings.host, settings.port)) {
    exit(EXIT_FAILURE);
  }
}

/// Tries to authenticate with the server
void App_authenticate() {
  if (client == NULL) {
    Error("Client not initialised");
    exit(EXIT_FAILURE);
  }

  char nickname[kMaxNicknameSize + sizeof(wchar_t)] = {};

  if (strlen(settings.user) > 0) {
    // Use the nickname provided from the command line...
    strncpy(nickname, settings.user, kMaxNicknameSize);
  } else {
    // ...or read from the input as Unicode (UCS)
    wchar_t inputNickname[kMaxNicknameInputBuffer] = {};
    fprintf(stdout, "   〉Please, enter a nickname (max %d chars): ", kMaxNicknameLength);
    fflush(stdout); // or some clients won't display the above
    // fgetws() reads length -1 characters and includes the new line
    if (!fgetws(inputNickname, kMaxNicknameInputBuffer, stdin)) {
      fprintf(stdout, "Unable to read nickname\n");
      exit(EXIT_FAILURE);
    }

    // Remove unwanted trailing spaces and new line characters
    wchar_t *trimmedNickname = wtrim(inputNickname, NULL);

    // Convert into UTF-8
    wcstombs(nickname, trimmedNickname, kMaxNicknameSize + sizeof(wchar_t));
  }

  // Send to the server for authentication
  if (!Client_authenticate(client, nickname)) {
    exit(EXIT_FAILURE);
  }
}

/**
 * Listens to input from the server and updates the main chat log
 * It needs to be called after UIInit()
 */
void *App_listen(void *data) {
  (void)data;
  SOCKET server = Client_getSocket(client);
  MessageBuffer *buffer = Client_getBuffer(client);

  int received = 0;

  fd_set reads;
  FD_ZERO(&reads);
  FD_SET(server, &reads);

  int msec = 200;
  struct timespec ts = {
    .tv_sec = msec / 1000,
    .tv_nsec = (msec % 1000) * 1000000
  };

  Info("Starting listening thread...");
  while(!terminate) {
    if (!SOCKET_isValid(server)) break;
    if (select(server + 1, &reads, 0, 0, NULL) < 0) {
      // Ignore a signal received before timeout, will be managed by handler
      if (SOCKET_getErrorNumber() == EINTR) continue;

      Error(
        "select() failed. (%d): %s",
        SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber())
      );
      terminate = true;
      break;
    }
    if (FD_ISSET(server, &reads)) {
      received = Client_receive(client);
      if (received <= 0) {
        terminate = true;
        break;
      }
      while (true) {
        C2HMessage *response = C2HMessage_get(buffer);
        if (response == NULL) break;
        // Push the message to be read by the main thread
        CQueue_push(messages, response, sizeof(C2HMessage));
        C2HMessage_free(&response);
      }

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
  pthread_kill(mainThreadID, SIGTERM);

  // Clean exit
  Info("Closing listening thread");
  pthread_exit(NULL);
}

/**
 * Handler to be injected into the input loop
 * to manage chatlog updates
 */
void App_updateHandler() {
  while (true) {
    // Item content (void*) is actually a C2HMessage*,
    // length is sizeof(C2HMessage)
    QueueData *item = CQueue_tryPop(messages);
    if (item == NULL) break;
    UILogMessage(item->content);
    QueueData_free(&item);
  }
}

/**
 * Starts the application infinite loop
 * It needs to be called after UIInit()
 */
void App_run() {
  // Initialise the thread id in order to receive messages
  mainThreadID = pthread_self();

  wchar_t buffer[kMaxMessageLength] = {};
  while (!terminate) {
    int res = UIInputLoop(buffer, kMaxMessageLength, App_updateHandler);
    if (res > 0) {
      // Message to be sent to server
      // Clean input buffer
      wchar_t *trimmedBuffer = wtrim(buffer, NULL);

      // Convert it into UTF-8
      char messageBuffer[kBufferSize] = {};
      wcstombs(messageBuffer, trimmedBuffer, kBufferSize);

      // Send it to the server if non empty
      size_t messageBufferLength = strlen(messageBuffer);
      if (messageBufferLength > 0) {
        C2HMessage *message = C2HMessage_createFromString(
          messageBuffer,
          messageBufferLength
        );
        if (message == NULL) {
          Error("Received NULL message");
          continue;
        }
        if (message->type == kMessageTypeQuit) {
          C2HMessage_free(&message);
          break;
        }

        int sent = Client_send(client, message);
        C2HMessage_free(&message);

        // If the connection drops, break and close
        if (sent < 0) break;
      }
    } else if (res == kUITerminate) {
      terminate = true;
      break;
    } else {
      Error("Unhandled input loop error: %s", strerror(errno));
      break;
    }
  }

  // Try a clean clean exit
  C2HMessage quit = { .type = kMessageTypeQuit };
  Client_send(client, &quit);
}

int App_start() {
  messages = CQueue_new();
  if (messages == NULL) {
    Error("Unable to initialise message queue: %s", strerror(errno));
    return EXIT_FAILURE;
  }

  // Set up event handlers
  App_catch(SIGINT, App_handleSignal);
  App_catch(SIGTERM, App_handleSignal);
  App_catch(SIGUSR2, App_handleSignal);
  App_catch(SIGWINCH, App_handleSignal);

  // Initialise NCurses UI engine
  UIInit();

  UISetStatus(
    "Connected to %s:%s - Hit F1 to quit",
    settings.host, settings.port
  );

  // Start a new thread that listens for messages from the server
  // and updates the chat log window
  if (pthread_create(&listeningThreadID, NULL, App_listen, NULL) != 0) {
    Error("Unable to start listening thread: %s", strerror(errno));
    return EXIT_FAILURE;
  }

  // Start the app infinite loop
  App_run();

  // Cleanup UI
  UIClean();

  // Wait for the listening thread to finish
  fprintf(stdout, "Disconnecting...");
  pthread_join(listeningThreadID, NULL);

  // Clean Exit
  fprintf(stdout, "Bye!\n");
  return EXIT_SUCCESS;
}
