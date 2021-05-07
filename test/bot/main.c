/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "client.h"

static bool terminate = false;

enum {kBufferSize = 1024};

const int kMaxBots = 7;

char *host = NULL;
char *port = NULL;

char messages[1024][100];

// Called within a thread function, hides that thread from signals
void maskSignal() {
  sigset_t mask;
  sigemptyset(&mask);
#if defined(__linux__)
  sigaddset(&mask, SIGRTMIN+3);
#endif
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void Bot_stop(int signal) {
  terminate = true;
  printf("Received signal %d in thread %lu\n", signal, (unsigned long)pthread_self());
}

// Catch interrupt signals
int Bot_catch(int sig, void (*handler)(int)) {
   struct sigaction action;
   action.sa_handler = handler;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   return sigaction (sig, &action, NULL);
}

void* RunBot(void* data) {
  int *id = (int *)data;

  // Create a chat client
  C2HatClient *bot = Client_create();
  if (bot == NULL) {
    fprintf(stderr, "[%d] Bot client creation failed\n", *id);
    return NULL;
  }

  // Try to connect
  if (!Client_connect(bot, host, port)) {
    fprintf(stderr, "[%d] Connection failed\n", *id);
    Client_destroy(&bot);
    return NULL;
  }

  // Choose your nickname
  char nickname[50] = {0};
  sprintf(nickname, "Bot %d", *id);

  printf("Starting Bot thread %d: %lu\n", *id, (unsigned long)pthread_self());

  // Authenticate
  if (!Client_authenticate(bot, nickname)) {
    fprintf(stderr, "[%s] Authentication failed\n", nickname);
    Client_destroy(&bot);
    return NULL;
  }

  // Initialise random engine
  srand(time(NULL));

  // Prepare for the message loop
  fd_set reads;
  FD_ZERO(&reads);
  SOCKET server = Client_getSocket(bot);
  FD_SET(server, &reads);

  // Set connection timeout
  struct timeval timeout;
  timeout.tv_sec = 5;
  timeout.tv_usec = 0; // micro seconds

  // Start the message loop
  while (!terminate) {
    if (!SOCKET_isValid(server)) break;
    int result = select(server + 1, &reads, 0, 0, &timeout);
    if (result < 0) {
      if (SOCKET_getErrorNumber() == EINTR) break; // Signal received before timeout
      fprintf(stderr, "[%s] select() failed. (%d): %s\n", nickname, SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      break;
    }

    // Server didn't respond on time
    if (result == 0) {
      fprintf(stderr, "[%s] select() timeout expired. (%d): %s\n", nickname, SOCKET_getErrorNumber(), strerror(SOCKET_getErrorNumber()));
      break;
    }

    if (FD_ISSET(server, &reads)) {
      // We have data in a socket
      char read[kBufferSize] = {0};
      int received = Client_receive(bot, read, kBufferSize);
      if (received <= 0) {
        break;
      }
      // Print up to byte_received from the server
      printf("[%s/server]: %.*s\n", nickname, received, read);
    }

    // Throw a dice to send a message
    int probability = rand() % 100;
    if (probability < 30) {
      int messageID = rand() % 100;
      char message[1024] = {0};
      snprintf(message, 1023, "/msg %s", messages[messageID]);
      int sent = Client_send(bot, message, strlen(message) + 1);
      if (sent <= 0) {
        fprintf(stderr, "[%s] Unable to send message: %s\n", nickname, strerror(errno));
      }
    }
    sleep(1);
  }

  // Signal received, prepare to close
  printf("[%s] Closing connection...\n", nickname);
  int sent = Client_send(bot, "/quit", strlen("/quit") + 1);
  if (sent <= 0) {
    fprintf(stderr, "[%s] Unable close connection: %s\n", nickname, strerror(errno));
  }
  FD_CLR(server, &reads);
  Client_destroy(&bot);
  return id;
}

/// Load the test messages from the file
void LoadMessages() {
  FILE *fd = fopen("test/bot/messages.txt", "r");
  if (!fd) {
    fprintf(stderr, "Unable to open messages file: %s\n", strerror(errno));
    exit(1);
  }
  for (int i = 0; i < 100; ++i) {
    memset(messages[i], 0, 1024);
    fgets(messages[i], 1023, fd);
    // Remove last new line character
    *(messages[i] + strlen(messages[i]) -1) = 0;
  }
  fclose(fd);
}

/// Initialise sockets (win only)
void BotInit() {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    exit(EXIT_FAILURE);
  }
#endif
}

/// Cleanup sockets and return (win only)
int BotCleanup(int result) {
#if defined(_WIN32)
  WSACleanup();
#endif
  return result;
}

int main(int argc, char const *argv[]) {
  BotInit();

  if (argc < 3) {
    fprintf(stderr, "Usage: %s hostname port\n", argv[0]);
    return 1;
  }

  host = (char *)argv[1];
  port = (char *)argv[2];

  LoadMessages();

  Bot_catch(SIGINT, Bot_stop);
  Bot_catch(SIGTERM, Bot_stop);

  pthread_t threadId[kMaxBots];
  int values[kMaxBots];

  // Start new threads to process, each with a different input value
  for(int i = 0; i < kMaxBots; i++) {
    values[i] = i;
    pthread_create(&threadId[i], NULL, RunBot, &values[i]);
  }

  printf("Main loop... %lu\n", (unsigned long)pthread_self());

  printf("Terminating...\n");

  // Wait for all the threads to finish and close
  // TODO: if some threads are already terminated,
  // trying to join them by passing a pointer causes a segmentation fault:
  // try to use a COMPLETE flag so that we can only join running threads
  for(int j = 0; j < kMaxBots; j++) {
    // int *id = NULL;
    // int res = pthread_join(threadId[j], (void**)&id);
    int res = pthread_join(threadId[j], NULL);
    switch(res) {
      case 0:
        // printf("Joined %d, (%d)\n", j, *id);
        printf("Bot %d joined!\n", j);
      break;
      case EINVAL:
        fprintf(stderr, "Unable to join Bot %d: thread not joinable\n", j);
      break;
      case ESRCH:
        fprintf(stderr, "Unable to join Bot %d: thread not found\n", j);
      break;
      case EDEADLK:
        fprintf(stderr, "Unable to join Bot %d: possible deadlock\n", j);
      break;
      default:
        fprintf(stderr, "Unable to join Bot %d: %s\n", j, strerror(errno));
      break;
    }
  }

  printf("Bye!\n");
  return BotCleanup(EXIT_SUCCESS);
}
