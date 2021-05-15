#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include <pthread.h>

#include "ui.h"
#include "client.h"
#include "message/message.h"

enum {
  kBufferSize = 1024 ///< Max size of data that can be sent, including the NULL terminator
};

static bool terminate = false;

void NetInit();
int NetCleanup(int result);
int App_catch(int sig, void (*handler)(int));
void App_stop(int signal);
void *Client_listen(void *client);

int main(int argc, char const *argv[]) {
  NetInit();

  if (argc < 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    return 1;
  }

  const char * host = argv[1];
  const char * port = argv[2];

  // Create chat client
  C2HatClient *app = Client_create();
  if (app == NULL) {
    fprintf(stderr, "Client creation failed\n");
    return NetCleanup(EXIT_FAILURE);
  }

  // Try to connect
  if (!Client_connect(app, host, port)) {
    fprintf(stderr, "Connection failed\n");
    Client_destroy(&app);
    return NetCleanup(EXIT_FAILURE);
  }

  // Authenticate
  char nickname[30] = {0};
  fprintf(stdout, "Please, enter a nickname: ");
  if (!fgets(nickname, 30, stdin)) {
    fprintf(stderr, "Unable to authenticate\n");
    Client_destroy(&app);
    return NetCleanup(EXIT_FAILURE);
  }
  // Remove newline from nickname
  char *end = nickname + strlen(nickname) -1;
  *end = 0;
  if (!Client_authenticate(app, nickname)) {
    Client_destroy(&app);
    return NetCleanup(EXIT_FAILURE);
  }

  UIInit();

  char connectionStatus[120] = {0};
  int statusMessageLength = sprintf(connectionStatus, "Connected to %s:%s - Hit F1 to quit", host, port);
  UISetStatusMessage(connectionStatus, statusMessageLength);

  App_catch(SIGINT, App_stop);
  App_catch(SIGTERM, App_stop);
  App_catch(SIGWINCH, UIResizeHandler);

  // Start new thread that receives from server and displays in chat window
  pthread_t listeningThreadID = 0;
  pthread_create(&listeningThreadID, NULL, Client_listen, app);

  while(!terminate) {
    UILoopInit();

    // Request user input
    char buffer[kBufferSize] = {0};
    int inputSize = UIGetUserInput(buffer, kBufferSize);
    // User pressed F1 or other exit commands
    if (inputSize < 0) {
      terminate = true;
      break;
    }

    // Format it as a chat message and send it to the server
    char message[kBufferSize] = {0};
    int messageType = Message_getType(buffer);

    if (messageType == kMessageTypeQuit) break;

    // If the input is not a command, wrap it into a message command
    if (!messageType) {
      Message_format(kMessageTypeMsg, message, kBufferSize, "%s", buffer);
    } else {
      // Send the message as is
      memcpy(message, buffer, inputSize);
    }
    int sent = Client_send(app, message, strlen(message) + 1);

    // If the connection drops, break and close
    if (sent < 0) break;
  }

  // Try a clean clean exit
  Client_send(app, "/quit", strlen("/quit") + 1);

  // Cleanup UI
  UIClean();

  // Join other threads
  fprintf(stdout, "Disconnecting...\n");
  pthread_join(listeningThreadID, NULL);

  // Exit
  Client_destroy(&app);
  fprintf(stdout, "Bye!\n");
  return NetCleanup(EXIT_SUCCESS);
}

/// Initialise sockets (win only)
void NetInit() {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    exit(EXIT_FAILURE);
  }
#endif
}

/// Cleanup sockets and return (win only)
int NetCleanup(int result) {
#if defined(_WIN32)
  WSACleanup();
#endif
  return result;
}

/// Sets the termination flag on SIGINT or SIGTERM
void App_stop(int signal) {
  (void)(signal); // Disable unused parameter warning
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

/// Listen to the server and displays data on the chat window
void *Client_listen(void *client) {
  C2HatClient *app = (C2HatClient *) client;
  SOCKET server = Client_getSocket(app);

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
      received = Client_receive(app, buffer, kBufferSize);
      if (received <= 0) {
        terminate = true;
        break;
      }
      UILogMessage(buffer, received);
    }
    // Sleep for a bit,
    // will allow for the termination signal to be recognised
    nanosleep(&ts, NULL);
  }
  FD_CLR(server, &reads);
  SOCKET_close(server);
  pthread_exit(NULL);
}
