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

#include "cqueue.h"

#include <string.h>
#include <stdlib.h>

/**
 * Creates a new Concurrent Queue and returns its pointer
 */
CQueue *CQueue_new() {
  // Create the container concurrent CQueue
  CQueue *this = (CQueue *)calloc(1, sizeof(CQueue));
  if (this == NULL) return NULL;

  // Create the included standard queue
  this->queue = Queue_new();
  if (this->queue == NULL) {
    free(this); // to avoid memory leeks
    return NULL;
  }

  // Initialise the mutexes (0 on success)
  if (pthread_mutex_init(&(this->lock), NULL)
    || pthread_cond_init(&(this->condition), NULL)) {

    // Free everything on error...
    Queue_free(&(this->queue));
    memset(this, 0, sizeof(CQueue));
    free(this);
    return NULL;
  }
  return this;
}

/**
 * Destroys a Concurrent Queue and all its contents
 */
void CQueue_free(CQueue **this) {
  if (this != NULL) {
    if ((*this)->queue != NULL) {
      // Free the internal standard queue
      Queue_free(&((*this)->queue));
    }

    // Clean the mutex and condition
    pthread_mutex_destroy(&((*this)->lock));
    pthread_cond_destroy(&((*this)->condition));

    // Clean the container queue
    memset(*this, 0, sizeof(CQueue));
    free(*this);
    *this = NULL;
  }
}

/**
 * Adds an object at the end of the queue
 */
bool CQueue_push(CQueue *this, void *data, size_t length) {
  if (!this) return false;
  pthread_mutex_lock(&(this->lock));
  bool success = Queue_enqueue(this->queue, data, length);
  pthread_mutex_unlock(&(this->lock));

  // Listening threads are notified that
  // the queue contains data
  pthread_cond_signal(&(this->condition));
  return success;
}

/**
 * Waits for the queue to contain data and pops
 * the first available object
 */
QueueData *CQueue_waitAndPop(CQueue *this) {
  pthread_mutex_lock(&(this->lock));
  if (Queue_empty(this->queue)) {
    pthread_cond_wait(&(this->condition), &(this->lock));
  }
  QueueData *item = Queue_dequeue(this->queue);
  pthread_mutex_unlock(&(this->lock));
  return item;
}

/**
 * Tries to pop the first object from the queue
 * without waiting
 */
QueueData *CQueue_tryPop(CQueue *this) {
  pthread_mutex_lock(&(this->lock));
  if (Queue_empty(this->queue)) {
    pthread_mutex_unlock(&(this->lock));
    return NULL;
  }
  QueueData *item = Queue_dequeue(this->queue);
  pthread_mutex_unlock(&(this->lock));
  return item;
}
