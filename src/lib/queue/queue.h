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

#ifndef QUEUES_H
#define QUEUES_H

#include <stdbool.h>

/**
 * A queue is a minimal struct that keeps information
 * about its nodes. The Queue structure is an opaque type,
 * its members should be accessed only by the exposed functions.
 */
typedef struct _Queue Queue;

/**
 * QueueData is a wrapper structure for each item stored within
 * a queue node. It contains a void pointer to the actual data
 * and the data size. The external program is responsible of
 * passing the right data length and perform type conversion.
 */
typedef struct {
  void *content; ///< Pointer to the actual data
  size_t length; ///< Size of the data
} QueueData;

/// Creation and disposal


// Creates a new Queue and returns its pointer
Queue *Queue_new();

// Destroys a queue and all its nodes
// The pointer to a queue pointer should passed
// Queue * myqueue = Queue_new();
// Queue_free(&myqueue)
void Queue_free(Queue **);

// Destroy a QueueData object pointer
// It needs to be used after Queue_dequeue
// It DOES NOT need to be used after Queue_peak and Queue_lpeak
void QueueData_free(QueueData **);

/// Utility


// Checks if a queue is empty
bool Queue_empty(const Queue *);


// Queue management

// Add an element at the end of the queue (Last IN)
bool Queue_enqueue(Queue *, void*, size_t);

// Returns and remove the first element of the queue
// The QueueData item returned is a copy and NEEDS to be freed with QueueData_free
QueueData *Queue_dequeue(Queue *);

// Returns the first element of the queue without removing it
// The QueueData item returned is a link and DOES NOT NEED to be freed with QueueData_free
QueueData *Queue_peek(Queue *);

// Returns the last element of the queue without removing it
// The QueueData item returned is a link and DOES NOT NEED to be freed with QueueData_free
QueueData *Queue_lpeek(Queue *);

// Deletes all the elements of the queue
bool Queue_purge(Queue *);

// Returns the length of the queue
int Queue_length(Queue *);

#endif
