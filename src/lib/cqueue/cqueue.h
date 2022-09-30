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

#ifndef CQUEUES_H
#define CQUEUES_H

  #include <pthread.h>
  #include <stdbool.h>

  #include "queue/queue.h"

  typedef struct {
    Queue *queue;
    pthread_mutex_t lock;
    pthread_cond_t condition;
  } CQueue;

  /**
   * Creates a new Concurrent Queue and returns its pointer
   */
  CQueue *CQueue_new();

  /**
   * Destroys a concurrent Queue and all its data
   */
  void CQueue_free(CQueue **this);

  /**
   * Adds an object at the end of the concurrent queue and
   * notifies listening threads
   *
   * The object content is duplicated, but if it contains any
   * dynamically allocated pointers, they have to be feed
   * when the object is popped from the queue
   */
  bool CQueue_push(CQueue *this, void *data, size_t length);

  /**
   * Waits for data to be present in the queue and
   * pops the first one
   *
   * The calling program needs to know the object type
   * in order to cast it and use it
   */
  QueueData *CQueue_waitAndPop(CQueue *this);

  /**
   * Pops the first object from the queue if not empty
   *
   * The calling program needs to know the object type
   * in order to cast it and use it
   */
  QueueData *CQueue_tryPop(CQueue *this);
#endif
