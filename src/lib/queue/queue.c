/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdlib.h>
#include <string.h>

#include "queue.h"

/**
 * A QueueNode is a generic struct with a void pointer
 * so that it can hold different type of data
 */
typedef struct _QNode QueueNode;

/// Internal structure, alias for QueueNode
typedef struct _QNode {
  QueueNode *next; ///< Pointer to the next node in the queue
  QueueData data;  ///< Pointer to the content of the node
} QueueNode;

/**
 * Structure that keep tracks of QueueNodes
 */
typedef struct _Queue {
  QueueNode *first; ///< Pointer to the first item in the queue
  QueueNode *last;  ///< Pointer to the last item in the queue
  int length; ///< Length of the queue
} Queue;

/// Creation and disposal


// Creates a new Queue and returns its pointer
Queue *Queue_new() {
  Queue *this = (Queue *)calloc(sizeof(Queue), 1);
  if (this == NULL) return NULL;
  return this;
}

// Destroys a queue and all its nodes
void Queue_free(Queue **this) {
  // Free the queue
  if (this != NULL) {
    // Purge the queue from all nodes
    Queue_purge(*this);
    // This will erase the memory
    memset(*this, 0, sizeof(Queue));  // Also '\0' is fine
    // Then free the pointer to which this queue is pointing
    // which is the actual pointer to the queue
    free(*this);
    *this = NULL;
  }
}


/// Utility


// Checks if a queue is empty
bool Queue_empty(const Queue *this) {
  return (this->length == 0 || this->first == NULL || this->last == NULL);
}

// Node management

// Creates a new empty QueueNode and returns its pointer
QueueNode *QueueNode_new(void *content, size_t length) {
  QueueNode *this = (QueueNode *)calloc(sizeof(QueueNode), 1);
  if (this == NULL) return NULL;
  // Allocating memory for the node data
  this->data.content = calloc(length, 1);
  if (this->data.content == NULL) return NULL;
  this->data.length = length;
  memcpy(this->data.content, content, this->data.length);
  return this;
}

// Destroys a QueueNode
void QueueNode_free(QueueNode **this) {
  // Free the node
  if (this != NULL) {
    // Cleanup the memory for the data
    memset((*this)->data.content, 0, (*this)->data.length);
    // Free the data pointer BEFORE cleaning up the node
    free((*this)->data.content);

    memset(*this, 0, sizeof(QueueNode)); // Also '\0' is fine
    // Free the pointer to which this is pointing
    // which is the actual pointer to the node
    free(*this);
    *this = NULL;
  }
}

// Destroys a QueueData object
void QueueData_free(QueueData **this) {
  // Free the data
  if (this != NULL) {
    // Cleanup the memory for the content
    memset((*this)->content, 0, (*this)->length);
    // Free the content pointer BEFORE cleaning up the object
    free((*this)->content);

    memset(*this, 0, sizeof(QueueData)); // Also '\0' is fine
    // Free the pointer to which this is pointing
    // which is the actual pointer to the node
    free(*this);
    *this = NULL;
  }
}

// Queue management

// Add an element at the end of the queue (Last IN)
bool Queue_enqueue(Queue *this, void *data, size_t length) {
  QueueNode *item = QueueNode_new(data, length);

  // Inserting the first item
  if (Queue_empty(this)) {
    this->first = item;
    this->last = item;
    this->length += 1;
    return true;
  }

  // Append the item to the last one
  this->last->next = item;

  // Adjust the last pointer
  this->last = item;
  this->length += 1;
  return true;
}

// Returns and remove the first element of the queue
QueueData *Queue_dequeue(Queue *this) {
  if (!Queue_empty(this)) {

    // Take the first element
    QueueNode *item = this->first;

    // Detach it from the queue
    this->first = this->first->next; // which can be null

    // Copy node data
    // NOTE: memory is copied, it needs to be freed somewhere
    // First, allocate memory for the QueueData structure pointer
    QueueData *data = (QueueData *)calloc(sizeof(QueueData), 1);
    if (!data) return NULL;
    data->length = item->data.length;
    // Then, allocate memory for the QueueData content
    data->content = calloc(data->length, 1);
    if (!data->content) return NULL;
    // Copy the node data
    memcpy(data->content, item->data.content, item->data.length);

    // Deletes the node
    QueueNode_free(&item);

    this->length -= 1;

    // Return its data
    return data;
  }
  return NULL;
}

// Returns the first element of the queue without removing it
QueueData *Queue_peek(Queue *this) {
  if (!Queue_empty(this)) {
    // Returns the payload if the first element
    return &(this->first->data);
  }
  return NULL;
}

// Returns the last element of the queue without removing it
QueueData *Queue_lpeek(Queue *this) {
  if (!Queue_empty(this)) {
    // Returns the payload if the last element
    return &(this->last->data);
  }
  return NULL;
}

// Returns the length of the queue
int Queue_length(Queue *this) {
  return this->length;
}

// Deletes all the elements of the queue
bool Queue_purge(Queue *this) {
  if (!Queue_empty(this)) {
    QueueNode *item;
    while (this->first != NULL) {
      // Extract the first item
      item = this->first;

      // Reposition the first pointer
      this->first = item->next;

      // Delete the item
      QueueNode_free(&item);
    }
    this->last = NULL;
    this->length = 0;
    return true;
  }
  return false;
}
