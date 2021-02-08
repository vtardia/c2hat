#ifndef QUEUES_H
#define QUEUES_H

#include <stdbool.h>


// A queue is a minimal struct that keeps information
// about its nodes. The Queue structure is an opaque type,
// its members should be accessed only by the exposed functions.
typedef struct _Queue Queue;

// QueueData is a wrapper structure for each item stored within
// a queue node. It contains a void pointer to the actual data
// and the data size. The external program is responsible of
// passing the right data length and perform type conversion.
typedef struct {
  void *content;
  size_t length;
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
