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
