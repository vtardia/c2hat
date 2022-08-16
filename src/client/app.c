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

/// Keeps track of the listening thread id so other threads can send signals
static pthread_t listeningThreadID = 0;

/// Reference to a C2Hat Client object
static C2HatClient *client = NULL;

/// Contains a copy of the chat client settings
static ClientOptions settings = {};

/// Cleanup resources and exit
int App_cleanup(int result) {
#if defined(_WIN32)
  WSACleanup();
#endif
  if (client != NULL) {
    fprintf(stdout, "Destroying client...");
    Client_destroy(&client);
    fprintf(stdout, "OK!\n");
  }
  return result;
}

/// Initialise the application resources
void App_init(ClientOptions *options) {
  if (client != NULL) return; // Already initialised

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
    exit(App_cleanup(EXIT_FAILURE));
  }

  // ...and try to connect
  if (!Client_connect(client, settings.host, settings.port)) {
    exit(App_cleanup(EXIT_FAILURE));
  }
}

/// Tries to authenticate with the server
void App_authenticate() {
  if (client == NULL) {
    fprintf(stderr, "Client not initialised\n");
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
      fprintf(stderr, "Unable to read nickname\n");
      exit(App_cleanup(EXIT_FAILURE));
    }

    // Remove unwanted trailing spaces and new line characters
    wchar_t *trimmedNickname = wtrim(inputNickname, NULL);

    // Convert into UTF-8
    wcstombs(nickname, trimmedNickname, kMaxNicknameSize + sizeof(wchar_t));
  }

  // Send to the server for authentication
  if (!Client_authenticate(client, nickname)) {
    exit(App_cleanup(EXIT_FAILURE));
  }
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
      received = Client_receive(client);
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

/**
 * Starts the application infinite loop
 * It needs to be called after UIInit()
 */
void App_run() {
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
    int sent = Client_send(client, message, strlen(message) + 1);

    // If the connection drops, break and close
    if (sent < 0) break;
  }

  // Try a clean clean exit
  Client_send(client, "/quit", strlen("/quit") + 1);
}

int App_start() {
  // To correctly display Advanced Character Set in UTF-8 environment
  setenv("NCURSES_NO_UTF8_ACS", "0", 1);

  // Initialise NCurses UI engine
  UIInit();
  char connectionStatus[kMaxStatusMessageSize] = {};
  int statusMessageLength = sprintf(
      connectionStatus, "Connected to %s:%s - Hit F1 to quit",
      settings.host, settings.port
  );
  UISetStatusMessage(connectionStatus, statusMessageLength);

  // Set up event handlers
  App_catch(SIGINT, App_terminate);
  App_catch(SIGTERM, App_terminate);
  App_catch(SIGWINCH, UIResizeHandler);
  App_catch(SIGUSR1, UITerminate);
  App_catch(SIGUSR2, UIQueueHandler);

  // Start a new thread that listens for messages from the server
  // and updates the chat log window
  pthread_create(&listeningThreadID, NULL, App_listen, NULL);

  // Start the app infinite loop
  App_run();

  // Cleanup UI
  UIClean();

  // Wait for the listening thread to finish
  fprintf(stdout, "Disconnecting...");
  pthread_join(listeningThreadID, NULL);

  // Clean Exit
  fprintf(stdout, "Bye!\n");
  return App_cleanup(EXIT_SUCCESS);
}
