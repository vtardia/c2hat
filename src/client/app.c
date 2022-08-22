/*
 * Copyright (C) 2021 Vito Tardia
 */

#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdatomic.h>

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

/// Initialise the application resources
void App_init(ClientOptions *options) {
  if (client != NULL) return; // Already initialised

  // Register shutdown function
  atexit(App_cleanup);

// Initialise sockets on Windows
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize\n");
    exit(EXIT_FAILURE);
  }
#endif

  // Copy the settings for later use
  memcpy(&settings, options, sizeof(ClientOptions));

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

/// Sets the termination flag on SIGINT or SIGTERM
void App_terminate(int signal) {
  if (signal == SIGINT || signal == SIGTERM) {
    terminate = true;
    UITerminate();
  } else if (signal == SIGUSR2) {
    UIUpdateChatLog();
  } else if (signal == SIGSEGV) {
    // Should prevent terminal messup on crash
    if (!isendwin()) endwin();
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
      // char *response = NULL;
      while (true) {
        char *response = Message_get(buffer);
        if (response == NULL) break;
        // Push the message to be read by the main thread
        CQueue_push(messages, response, strlen(response) + 1);
        Message_free(&response);
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
 * Starts the application infinite loop
 * It needs to be called after UIInit()
 */
void App_run() {
  // Initialise the thread id in order to receive messages
  mainThreadID = pthread_self();

  wchar_t buffer[kMaxMessageLength] = {};
  while (!terminate) {
    int res = UIInputLoop(buffer, kMaxMessageLength);
    if (res > 0) {
      // Message to be sent to server
      // Clean input buffer
      wchar_t *trimmedBuffer = wtrim(buffer, NULL);

      // Convert it into UTF-8
      char messageBuffer[kBufferSize] = {};
      wcstombs(messageBuffer, trimmedBuffer, kBufferSize);

      int messageType = Message_getType(messageBuffer);
      if (messageType == kMessageTypeQuit) break;

      // If the input is not a command, wrap it into a message payload
      char message[kBufferSize] = {};
      if (!messageType) {
        Message_format(kMessageTypeMsg, message, kBufferSize, "%s", messageBuffer);
      } else {
        // Send the message as is
        memcpy(message, messageBuffer, res);
      }
      int sent = Client_send(client, message, strlen(message) + 1);

      // If the connection drops, break and close
      if (sent < 0) break;
    } else if (res == kUITerminate) {
      terminate = true;
      break;
    } else if (res == kUIUpdate) {
      while (true) {
        QueueData *item = CQueue_tryPop(messages);
        if (item == NULL) break;
        UILogMessage(item->content, item->length);
        QueueData_free(&item);
      }
    } else if (res == kUIResize) {
      // Trigger a resize
    }
  }

  // Try a clean clean exit
  Client_send(client, "/quit", strlen("/quit") + 1);
}

int App_start() {
  messages = CQueue_new();
  if (messages == NULL) {
    Error("Unable to initialise message queue: %s\n", strerror(errno));
    return EXIT_FAILURE;
  }

  // Set up event handlers
  App_catch(SIGINT, App_terminate);
  App_catch(SIGTERM, App_terminate);
  App_catch(SIGUSR2, App_terminate);
  // App_catch(SIGWINCH, UIResizeHandler);

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
