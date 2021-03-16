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

pthread_mutex_t termLock = PTHREAD_MUTEX_INITIALIZER;

// Called within a thread function, hides that thread from signals
void maskSignal() {
  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGRTMIN+3);
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void Bot_stop(int signal) {
  pthread_mutex_lock(&termLock);
  terminate = true;
  printf("Received signal %d in thread %lu\n", signal, pthread_self());
  pthread_mutex_unlock(&termLock);
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
  maskSignal();
  int *id = (int *)data;
  SOCKET server = Client_connect(host, port);
  if (server == -1) {
    fprintf(stderr, "Connection failed\n");
  #if defined(_WIN32)
    WSACleanup();
  #endif
    return NULL;
  }

  char nick[50] = {0};
  sprintf(nick, "Bot %d", *id);

  printf("Bot Thread %d... %lu\n", *id, pthread_self());

  // Wait for the OK signal from the server
  char read[kBufferSize] = {0};
  int received = Client_receive(server, read, kBufferSize);
  if (received > 0) {
    if (strncmp(read, "/ok", 3) != 0) {
      printf("The server refused the connection\n");
      SOCKET_close(server);
      return NULL;
    }
    printf("[%s/server]: %s\n", nick, (read + 4));
  } else {
    SOCKET_close(server);
    return NULL;
  }

  // Wait for the authentication prompt
  memset(read, 0, kBufferSize);
  received = Client_receive(server, read, kBufferSize);
  if (received > 0) {
    if (strncmp(read, "/nick", 5) != 0) {
      printf("Unable to autenticate\n");
      SOCKET_close(server);
      return NULL;
    }
    printf("[%s/server]: %s\n", nick, (read + 6));
  } else {
    SOCKET_close(server);
    return NULL;
  }

  // Send Nick
  char nickMessage[100] = {0};
  sprintf(nickMessage, "/nick %s", nick);
  Client_send(server, nickMessage, strlen(nickMessage) + 1);
  memset(read, 0, kBufferSize);
  received = Client_receive(server, read, kBufferSize);
  if (received > 0) {
    if (strncmp(read, "/ok", 3) != 0) {
      printf("[%s] Authentication failed\n", nick);
      SOCKET_close(server);
      return NULL;
    }
    printf("[%s/server]: %s\n", nick, (read + 4));
  } else {
    SOCKET_close(server);
    return NULL;
  }

  srand(time(NULL));
  while (!terminate) {
    // Initialise the socket set
    fd_set reads;
    FD_ZERO(&reads);
    // Add our listening socket
    FD_SET(server, &reads);

    if (select(server + 1, &reads, 0, 0, NULL) < 0) {
      if (SOCKET_getErrorNumber() == EINTR) break; // Signal received before timeout
      fprintf(stderr, "select() failed. (%d): %s\n", SOCKET_getErrorNumber(), gai_strerror(SOCKET_getErrorNumber()));
      break;
    }

    if (FD_ISSET(server, &reads)) {
      // We have data in a socket
      char read[kBufferSize] = {0};
      int received = Client_receive(server, read, kBufferSize);
      if (received <= 0) {
        break;
      }
      // Print up to byte_received from the server
      printf("[%s/server]: %.*s\n", nick, received, read);
    }

    // Throw a dice to send a message
    int probability = rand() % 100;
    if (probability < 30) {
      int messageID = rand() % 100;
      char message[1024] = {0};
      snprintf(message, 1023, "/msg %s", messages[messageID]);
      int sent = Client_send(server, message, strlen(message) + 1);
      if (sent <= 0) {
        fprintf(stderr, "[%s] Unable to send message: %s\n", nick, strerror(errno));
      }
    }
    sleep(5);
  }

  Client_send(server, "/quit", strlen("/quit") + 1);

  printf("[%s] Closing connection...", nick);
  SOCKET_close(server);

  return id;
}

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

int main(int argc, char const *argv[]) {
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

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

  // Start new threads to process func1, each with a different input value
  for(int i = 0; i < kMaxBots; i++) {
    values[i] = i;
    pthread_create(&threadId[i], NULL, RunBot, &values[i]);
    // pthread_detach(threadId[i]);
  }

  printf("Main loop... %lu\n", pthread_self());

  while (!terminate) {
    sleep(1);
  }

  printf("Terminating...\n");

  // Wait for all the threads to finish and close
  for(int j = 0; j < kMaxBots; j++) {
    pthread_join(threadId[j], NULL);
    // pthread_kill(threadId[j], 15);
  }

#if defined(_WIN32)
  WSACleanup();
#endif
  printf("Bye!\n");
  return 0;
}
