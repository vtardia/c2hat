/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "list.h"

// Creates a new empty List and returns its pointer
List *List_new() {
  List *this = (List *)calloc(sizeof(List), 1);
  if (this == NULL) return NULL;
  return this;
}

// Destroys a list and all its nodes
void List_free(List **this) {
  if (this != NULL) {

    // First walk the list and free all its nodes
    // We implicitely trust the developer that didn't mess up
    // with the length attribute of the list
    if (!List_empty(*this)) {
      while ((*this)->length > 0) {
        List_delete(*this, (*this)->length - 1);
      }
    }

    // Then free the list itself
    // This will erase the data in memory
    memset(*this, 0, sizeof(List));  // Also '\0' is fine
    // Free the pointer to which this is pointing
    // which is the actual pointer to the list
    free(*this);
    *this = NULL;
  }
}

// Tells if a list is empty
bool List_empty(const List *this) {
  return (this->length == 0 || this->first == NULL);
}

// Creates a new empty ListNode and returns its pointer
// The data container is allocated from the heap, and
// the content of the value is duplicated
ListNode *ListNode_new(ListData *value, size_t valueSize) {
  ListNode *this = (ListNode *)calloc(sizeof(ListNode), 1);
  if (this == NULL) return NULL;
  // Allocating memory for the node data
  this->data = calloc(valueSize, 1);
  if (this->data == NULL) return NULL;
  memcpy(this->data, value, valueSize);
  this->dataSize = valueSize;
  return this;
}

// Destroys a ListNode
void ListNode_free(ListNode **this) {
  // Free the node
  if (this != NULL) {
    // Cleanup the memory for the data
    memset((*this)->data, 0, (*this)->dataSize);
    // Free the data pointer BEFORE cleaning up the node
    free((*this)->data);

    // Cleanup the memory for the node
    memset(*this, 0, sizeof(ListNode)); // Also '\0' is fine
    // Free the pointer to which this is pointing
    // which is the actual pointer to the node
    free(*this);
    *this = NULL;
  }
}

// Insert a new node at the end of the list
bool List_append(List *this, ListData *value, size_t valueSize) {
  return List_insert(this, value, valueSize, this->length);
}

// Insert a new node at the beginning of the list
bool List_prepend(List *this, ListData *value, size_t valueSize) {
  return List_insert(this, value, valueSize, 0);
}

// Insert an element at the given zero based position
bool List_insert(List *this, ListData *value, size_t valueSize, int pos) {
  // We can insert only within the list limits
  if (pos >= 0 && pos <= this->length) {
    // Create the new node
    ListNode *item = ListNode_new(value, valueSize);

    // Get a handle to the first node to walk the list
    ListNode *cursor = this->first;

    if (cursor == NULL) {
      // The list was empty and we are inserting the first item
      this->first = item;
      this->current = item;
      this->last = item;
      this->length += 1;
      return true;
    }

    // The list is not empty, let's walk
    int i = 0;
    while (i < pos) {
      cursor = cursor->next;
      i++;
    }

    // We've reached past the last item
    // This happens when pos = list->length
    if (cursor == NULL) {
      // We are appending an item
      item->prev = this->last;
      this->last->next = item;
      this->last = item;
      this->length += 1;
      return true;
    }

    // Insert a generic node between first and last
    item->next = cursor;
    item->prev = cursor->prev;
    if (cursor->prev != NULL) {
      // When pos != 0, standard insert
      cursor->prev->next = item;
    } else {
      // When pos = 0, we're prepending
      this->first = item;
    }
    cursor->prev = item;
    this->length += 1;
    return true;
  }
  return false;
}

// Deletes a node the the given position
bool List_delete(List *this, int pos) {
  if (!List_empty(this) && pos >= 0 && pos < this->length) {
    int i = 0;

    // Start at first node
    ListNode *n = this->first;

    // Walk the list until pos -1
    while (i < pos) {
      n = n->next;
      i++;
    }

    // n is now the node to be deleted

    // Backup previous node
    ListNode *pn = n->prev;

    // Backup next node
    ListNode *nn = n->next;

    if (pn == NULL) {
      // We're deleting the first node, we need to fix the pointers
      this->current = this->first->next;
      this->first = this->current;
    } else {
      // Link n's next node to its previous
      pn->next = nn;
    }

    if (nn == NULL) {
      // We're deleting the last node, we need to the fix pointers
      this->last = n->prev;
    } else {
      // Link n's previous node to its next
      nn->prev = pn;
    }

    // n is now extracted from the list, we can delete it...
    ListNode_free(&n);
    // ...and update the length
    this->length -= 1;
    return (n == NULL); // If this is false false there is a leak somewhere
  }
  return false;
}

// Update the value of the None at the given position
bool List_update(List *this, ListData *value, size_t valueSize, int pos) {
  if (!List_empty(this) && pos >= 0 && pos < this->length) {
    int i = 0;

    // Start at first node
    ListNode *n = this->first;
    // Walk the list until pos -1
    while (i < pos) {
      n = n->next;
      i++;
    }

    // Cleanup and free the old data pointer
    memset(n->data, 0, n->dataSize);
    free(n->data);

    // Allocating memory for the new node data
    n->data = calloc(valueSize, 1);
    if (n->data == NULL) return false;
    memcpy(n->data, value, valueSize);
    n->dataSize = valueSize;
    return true;
  }
  return false;
}

// Returns the value of the current element of the list
// and moves the cursor forwards
ListData *List_next(List *this) {
  // Pick the current item from the list
  ListNode *current = this->current;

  if (current != NULL) {
    // Advance the current cursor
    this->current = current->next;
    // Return current data
    return current->data;
  }
  return NULL;
}

// Returns the value of the current element of the list
// and moves the cursor backwards
ListData *List_prev(List *this) {
  // Pick the current item from the list
  ListNode *current = this->current;

  if (current != NULL) {
    // Advance the current cursor
    this->current = current->prev;
    // Return current data
    return current->data;
  }
  return NULL;
}

// Returns the value of the first ListNode of the list
ListData *List_first(List *this) {
  if (!List_empty(this) && this->first != NULL) {
      return this->first->data;
  }
  return NULL;
}

// Returns the value of the last ListNode of the list
ListData *List_last(List *this) {
  if (!List_empty(this) && this->last != NULL) {
    return this->last->data;
  }
  return NULL;
}

// Rewinds the list cursor
bool List_rewind(List *this) {
  if (!List_empty(this)) {
    this->current = this->first;
    return true;
  }
  return false;
}

// Search a value within a list, return the index of the first match
int List_search(List *this, ListData *needle, size_t needleSize, int (*compare)(const ListData *, const ListData *, size_t)) {
  if (!List_empty(this)) {
    int pos = 0;
    ListNode *n = this->first;
    while (pos < this->length) {
      // First check size
      if (n->dataSize == needleSize) {
        // Then check content
        if (compare(n->data, needle, needleSize) == 0) {
          return pos;
        }
      }
      n = n->next;
      pos++;
    }
  }
  return -1;
}

// Returns the value of the item at the given position or NULL
ListData *List_item(List *this, int pos) {
  if (!List_empty(this) && pos >= 0 && pos < this->length) {
    int i = 0;
    ListNode *n = this->first;
    while (i < pos) {
      n = n->next;
      i++;
    }
    return n->data;
  }
  return NULL;
}

// Swap the content of two nodes
void Node_swap(ListNode *a, ListNode *b) {
  ListData * temp = a->data;
  size_t tempSize = a->dataSize;
  a->data = b->data;
  a->dataSize = b->dataSize;
  b->data = temp;
  b->dataSize = tempSize;
}

// Implement Quick Sort on a list
// Shamefully copied and adapted from https://www.flipcode.com/archives/Quick_Sort_On_Linked_List.shtml
// (No license specified)
void QuickSortList(ListNode *left, ListNode *right, int (*compare)(const ListData *, const ListData *, size_t)) {
  ListNode *start;
  ListNode *current;

  // If the left and right pointers are the same, then return
  if (left == right) return;

  // Set the Start and the Current item pointers
  start = left;
  current = start->next;

  // Loop forever (well until we get to the right)
  while (1) {
    // Select the smaller size for safe comparison
    size_t size = (start->dataSize <= current->dataSize) ? start->dataSize : current->dataSize;
    // If the start item is less then the right
    if (compare(start->data, current->data, size) < 0) {
      // Swap the items
      Node_swap(start, current);
    }
    // Check if we have reached the right end
    if (current == right) break;

    // Move to the next item in the list
    current = current->next;
  }

  // Swap the First and Current items
  Node_swap(left, current);

  // Save the current item
  ListNode *oldCurrent = current;

  // Check if we need to sort the left hand size of the Current point
  current = current->prev;
  if (current != NULL) {
    if ((left->prev != current) && (current->next != left))
      QuickSortList(left, current, compare);
  }

  // Check if we need to sort the right hand size of the Current point
  current = oldCurrent;
  current = current->next;
  if (current != NULL) {
    if ((current->prev != right) && (right->next != current))
      QuickSortList(current, right, compare);
  }
}

// Sort the list using QuickSort
void List_sort(List *this, int (*compare)(const ListData *, const ListData *, size_t)) {
  if (!List_empty(this)) {
    return QuickSortList(this->first, this->last, compare);
  }
}
