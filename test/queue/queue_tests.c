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

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "queue/queue.h"
#include "queue_tests.h"

// Test new, free, empty
void TestQueue_new() {
  Queue *myq = Queue_new();

  assert(myq != NULL);
  printf(".");
  assert(Queue_length(myq) == 0);
  printf(".");
  assert(Queue_empty(myq));
  printf(".");

  // Delete queue
  // We need to pass myq by reference (ie pointer to pointer)
  // otherwise a copy of the pointer will be freed, but the original
  // still remains
  Queue_free(&myq);
  assert(myq == NULL);
  printf(".");
}

void TestQueue_enqueue() {
  Queue *myq = Queue_new();

  // Appending to an empty queue
  assert(Queue_enqueue(myq, "Foo", 3));
  printf(".");

  assert(!Queue_empty(myq));
  printf(".");

  assert(Queue_length(myq) == 1);
  printf(".");

  QueueData *first = Queue_peek(myq);
  assert(strncmp(first->content, "Foo", first->length) == 0);
  printf(".");

  QueueData *last = Queue_lpeek(myq);
  assert(strncmp(last->content, "Foo", last->length) == 0);
  printf(".");

  assert(Queue_enqueue(myq, "Bar", 3));
  printf(".");

  assert(Queue_length(myq) == 2);
  printf(".");

  first = Queue_peek(myq);
  assert(strncmp(first->content, "Foo", first->length) == 0);
  printf(".");

  last = Queue_lpeek(myq);
  assert(strncmp(last->content, "Bar", last->length) == 0);
  printf(".");

  Queue_free(&myq);
}

void TestQueue_dequeue() {
  Queue *myq = Queue_new();

  // Fill the queue
  assert(Queue_enqueue(myq, "Foo", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Bar", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Baz", 3));
  printf(".");
  assert(Queue_length(myq) == 3);
  printf(".");

  QueueData *item = Queue_dequeue(myq);
  assert(strncmp(item->content, "Foo", item->length) == 0);
  QueueData_free(&item);
  printf(".");

  assert(Queue_length(myq) == 2);
  printf(".");

  item = Queue_dequeue(myq);
  assert(strncmp(item->content, "Bar", item->length) == 0);
  QueueData_free(&item);
  printf(".");

  assert(Queue_length(myq) == 1);
  printf(".");

  item = Queue_dequeue(myq);
  assert(strncmp(item->content, "Baz", item->length) == 0);
  QueueData_free(&item);
  printf(".");

  assert(Queue_length(myq) == 0);
  printf(".");

  assert(Queue_empty(myq));
  printf(".");

  // Trying to dequeue an empty que should result in null
  assert(Queue_dequeue(myq) == NULL);
  printf(".");
  Queue_free(&myq);
}

void TestQueue_peek() {
  Queue *myq = Queue_new();

  // Fill the queue
  assert(Queue_enqueue(myq, "Foo", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Bar", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Baz", 3));
  printf(".");
  assert(Queue_length(myq) == 3);
  printf(".");

  // Have a peek at the first item
  QueueData *item = Queue_peek(myq); // No need to free
  assert(strncmp(item->content, "Foo", item->length) == 0);
  printf(".");

  // Length should remain the same
  assert(Queue_length(myq) == 3);
  printf(".");

  // Dequeue (we already tested dequeue for length)
  item = Queue_dequeue(myq);
  assert(strncmp(item->content, "Foo", item->length) == 0);
  QueueData_free(&item);
  printf(".");

  item = Queue_peek(myq);
  assert(strncmp(item->content, "Bar", item->length) == 0);
  printf(".");

  Queue_free(&myq);
}

void TestQueue_lpeek() {
  Queue *myq = Queue_new();

  // Fill the queue
  assert(Queue_enqueue(myq, "Foo", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Bar", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Baz", 3));
  printf(".");
  assert(Queue_length(myq) == 3);
  printf(".");

  // Have a peek at the first item
  QueueData *item = Queue_lpeek(myq); // No need to free
  assert(strncmp(item->content, "Baz", item->length) == 0);
  printf(".");

  // Length should remain the same
  assert(Queue_length(myq) == 3);
  printf(".");

  // Dequeue (we already tested dequeue for length)
  item = Queue_dequeue(myq);
  assert(strncmp(item->content, "Foo", item->length) == 0);
  QueueData_free(&item);
  printf(".");

  item = Queue_lpeek(myq);
  assert(strncmp(item->content, "Baz", item->length) == 0);
  printf(".");

  Queue_free(&myq);
}

void TestQueue_purge() {
  Queue *myq = Queue_new();

  // Should not be able to purge an empty queue
  assert(!Queue_purge(myq));
  printf(".");

  // Fill the queue
  assert(Queue_enqueue(myq, "Foo", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Bar", 3));
  printf(".");
  assert(Queue_enqueue(myq, "Baz", 3));
  printf(".");
  assert(Queue_length(myq) == 3);
  printf(".");

  // Purge the whole queue and check the results
  assert(Queue_purge(myq));
  printf(".");
  assert(Queue_length(myq) == 0);
  printf(".");
  assert(Queue_empty(myq));
  printf(".");

  Queue_free(&myq);
}
