/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef LISTS_H
#define LISTS_H

#include <stdbool.h>

/**
 * Generic data type. We can redefine list data to
 * any type that suit our purpose
 */
typedef void ListData;

/**
 * A ListNode is a generic struct with a void pointer
 * so that it can hold different type of data
 */
typedef struct _Node ListNode;

/// Anonymous node structure, use ListNode
typedef struct _Node {
  ListNode *prev; ///< Pointer to the previous node in the list
  ListNode *next; ///< Pointer to the next node in the list
  ListData *data; ///< Pointer to the node data
  size_t dataSize; ///< Size of the node data
} ListNode;

/**
 * A list is a minimal struct that keeps information
 * about its nodes
 */
typedef struct {
  ListNode *first; ///< Pointer to the first list node
  ListNode *last;  ///< Pointer to the last list node
  ListNode *current; ///< Pointer to the current list node
  int length; ///< Length of the list
} List;


/// Creation and disposal


// Creates a new List and returns its pointer
List *List_new();

// Destroys a list and all its nodes
// The pointer to a list pointer should passed
// List * mylist = List_new();
// List_free(&mylist)
void List_free(List **);


/// Utility


// Checks if a list is empty
bool List_empty(const List *);

// Search a value within a list, return the index of the first match
// Like qsort() it needs a function to compare values because a node's data
// is a generic void pointer.
// The function will be used like compare(node->data, string_to_search)
int List_search(List*, ListData*, size_t, int (*compare)(const ListData *, const ListData *, size_t));

// Sort the list using QuickSort
void List_sort(List*, int (*compare)(const ListData *, const ListData *, size_t));


/// List Management


// Insert an element at the end of the list
bool List_append(List *, ListData *, size_t);

// Insert an element at the beginning of the list
bool List_prepend(List *, ListData *, size_t);

// Insert an element at the given zero based position
// The size parameter is the size of the data that need to be copied
bool List_insert(List *, ListData *, size_t, int);

// Deletes the node at the given position
bool List_delete(List*, int);

// Updates the node at the given position
bool List_update(List*, ListData*, size_t, int);


/// List Navigation


// Returns the value of the current element of the list
// and moves the cursor forwards
ListData *List_next(List*);

// Returns the value of the current element of the list
// and moves the cursor backwards
ListData *List_prev(List*);

// Returns the value of the first ListNode of the list
ListData *List_first(List*);

// Returns the value of the last ListNode of the list
ListData *List_last(List*);

// Returns the value of the item at the given position
// Please note: there is no size indication of the size
// of data pointed by the ListData* item. The calling program
// should take care of using the correct sizes.
ListData *List_item(List*, int);

// Rewinds the list cursor
bool List_rewind(List*);

#endif
