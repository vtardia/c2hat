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
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "list/list.h"
#include "list_tests.h"

// Compare list elements as strings
// NOTE: again sizes are important here
int strcompare(const ListData *a, const ListData *b, size_t size) {
  return strncmp((char *)a, (char*) b, size);
}

// Test new, free, empty
void TestList_new() {
  List *mylist = List_new();

  assert(mylist != NULL);
  printf(".");
  assert(mylist->first == NULL);
  printf(".");
  assert(mylist->last == NULL);
  printf(".");
  assert(mylist->current == NULL);
  printf(".");
  assert(mylist->length == 0);
  printf(".");
  assert(List_empty(mylist));
  printf(".");

  // Delete list
  // We need to pass mylist by reference (ie pointer to pointer)
  // otherwise a copy of the pointer will be freed, but the original
  // still remains
  List_free(&mylist);
  assert(mylist == NULL);
  printf(".");
}

// Creates a test list for all the other functions
List * TestList_mock() {
  List *mylist = List_new();
  List_append(mylist, "One", 3);
  List_append(mylist, "Two", 3);
  List_append(mylist, "Three", 5);
  List_append(mylist, "Four", 4);
  List_append(mylist, "Five", 4);
  return mylist;
}

// Display list content (mainly for debug)
void TestList_display(List *this) {
  ListData *value = NULL;
  if (this == NULL) {
    printf("List is empty\n");
    printf(":No nodes in the list to display\n");
  } else {
    List_rewind(this);
    printf("\nList contains %d items:\n", this->length);
    while ((value = List_next(this)) != NULL) {
      printf("%s\t", (char *)value);
    }
    printf("\n");
  }
}

// Test search
void TestList_search() {
  // Test search on an empty list
  List *mylist = List_new();
  assert(List_search(mylist, "Foo", 3, strcompare) == -1);
  printf(".");
  List_free(&mylist);

  // Test search on a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");
  assert(List_search(mylist, "One", 3, strcompare) == 0);
  printf(".");
  assert(List_search(mylist, "Three", 5, strcompare) == 2);
  printf(".");
  assert(List_search(mylist, "Five", 4, strcompare) == 4);
  printf(".");
  assert(List_search(mylist, "Unknown", 7, strcompare) == -1);
  printf(".");
  List_insert(mylist, "Four", 4, 1);
  assert(List_search(mylist, "Four", 4, strcompare) == 1);
  printf(".");
  List_free(&mylist);
}

// Test sort
void TestList_sort() {
  // Test sort on an empty list
  List *mylist = List_new();
  // Nothing and no errors should happen
  List_sort(mylist, strcompare);
  printf(".");
  List_free(&mylist);

  // Test search on a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  List_sort(mylist, strcompare);
  assert(strncmp(List_first(mylist), "Five", 4) == 0);
  assert(strncmp(List_item(mylist, 1), "Four", 4) == 0);
  assert(strncmp(List_item(mylist, 2), "One", 3) == 0);
  assert(strncmp(List_item(mylist, 3), "Three", 5) == 0);
  assert(strncmp(List_last(mylist), "Two", 3) == 0);
  List_free(&mylist);
}

// Test append
void TestList_append() {
  // Test appending on an empty list
  List *mylist = List_new();

  assert(List_append(mylist, "Foo", 3));
  printf(".");
  assert(!List_empty(mylist));
  printf(".");
  assert(mylist->length == 1);
  printf(".");

  assert(List_append(mylist, "Bar", 3));
  printf(".");
  assert(List_append(mylist, "Baz", 3));
  printf(".");
  assert(mylist->length == 3);
  printf(".");

  List_free(&mylist);

  // Test appending on a predefined list
  mylist = TestList_mock();
  assert(!List_empty(mylist));
  printf(".");
  assert(mylist->length == 5);
  printf(".");

  assert(List_append(mylist, "Foo", 3));
  printf(".");
  assert(mylist->length == 6);
  printf(".");

  // Test that we actually inserted the item at the end
  // IMPORTANT: we are implicitely treating the item as a char*
  // of known length, but in real life we don't always know the size
  // We should cast the return value to be precise
  assert(strncmp(
    (char *)List_item(mylist, 5), "Foo", 3
  ) == 0);
  printf(".");
  assert(strncmp(List_last(mylist), "Foo", 3) == 0);
  printf(".");

  List_free(&mylist);
}

// Test prepend
void TestList_prepend() {
  // Test prepending on an empty list
  List *mylist = List_new();

  assert(List_prepend(mylist, "Foo", 3));
  printf(".");
  assert(!List_empty(mylist));
  printf(".");
  assert(mylist->length == 1);
  printf(".");

  assert(List_prepend(mylist, "Bar", 3));
  printf(".");
  assert(List_prepend(mylist, "Baz", 3));
  printf(".");
  assert(mylist->length == 3);
  printf(".");

  // Test that we actually inserted the item at the begining
  assert(strncmp((char *)List_item(mylist, 0), "Baz", 3) == 0);
  printf(".");
  assert(strncmp(List_first(mylist), "Baz", 3) == 0);
  printf(".");

  List_free(&mylist);

  // Test prepending on a predefined list
  mylist = TestList_mock();
  assert(!List_empty(mylist));
  printf(".");
  assert(mylist->length == 5);
  printf(".");

  assert(List_prepend(mylist, "Foo", 3));
  printf(".");
  assert(mylist->length == 6);
  printf(".");

  // Test that we actually inserted the item at the beginning
  assert(strncmp((char *)List_item(mylist, 0), "Foo", 3) == 0);
  printf(".");
  assert(strncmp(List_first(mylist), "Foo", 3) == 0);
  printf(".");

  List_free(&mylist);
}

// Test insert
void TestList_insert() {
  // Test insertions on an empty list
  List *mylist = List_new();
  assert(List_empty(mylist));
  printf(".");
  assert(mylist->length == 0);
  printf(".");

  // Insert out of boundaries should fail
  assert(!List_insert(mylist, "Foo", 3, 1));
  printf(".");
  assert(!List_insert(mylist, "Foo", 3, -1));
  printf(".");
  assert(List_empty(mylist));
  printf(".");
  assert(mylist->length == 0);
  printf(".");

  // Insert of item at index zero should succeed
  assert(List_insert(mylist, "Foo", 3, 0));
  printf(".");
  assert(mylist->length == 1);
  printf(".");

  // Insert out of boundaries should fail
  assert(!List_insert(mylist, "Bar", 3, 2));
  printf(".");

  assert(List_insert(mylist, "Bar", 3, 0));
  printf(".");
  assert(mylist->length == 2);
  printf(".");

  assert(strncmp(List_item(mylist, 0), "Bar", 3) == 0);
  printf(".");
  assert(strncmp(List_item(mylist, 1), "Foo", 3) == 0);
  printf(".");

  // Inserting at the last available index is not append,
  // it will move the last element down
  assert(List_insert(mylist, "Baz", 3, 1));
  printf(".");
  assert(mylist->length == 3);
  printf(".");

  assert(strncmp(List_first(mylist), "Bar", 3) == 0);
  printf(".");
  assert(strncmp(List_item(mylist, 1), "Baz", 3) == 0);
  printf(".");
  assert(strncmp(List_last(mylist), "Foo", 3) == 0);
  printf(".");

  List_free(&mylist);

  // Test insertions on a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  // Should not insert beyond the limits
  assert(!List_insert(mylist, "Foo", 3, -1));
  printf(".");
  assert(!List_insert(mylist, "Foo", 3, 6));
  printf(".");

  // Can insert at the top
  assert(List_insert(mylist, "Foo", 3, 0));
  printf(".");
  assert(strncmp(List_first(mylist), "Foo", 3) == 0);
  printf(".");
  assert(mylist->length == 6);
  printf(".");
  assert(List_insert(mylist, "Bar", 3, 2));
  printf(".");
  assert(mylist->length == 7);
  printf(".");

  // Can insert at the bottom
  assert(List_insert(mylist, "Baz", 3, 6));
  printf(".");
  assert(strncmp(List_last(mylist), "Five", 4) == 0);
  printf(".");
  assert(strncmp(List_item(mylist, 6), "Baz", 3) == 0);
  printf(".");
  assert(mylist->length == 8);
  printf(".");

  // Can use insert as append by using the list->length as index
  assert(List_insert(mylist, "Eight", 5, 8));
  printf(".");
  assert(mylist->length == 9);
  printf(".");
  assert(strncmp(List_last(mylist), "Eight", 5) == 0);
  printf(".");

  List_free(&mylist);
}

// Test delete
void TestList_delete() {
  // Test deletions on an empty list
  List *mylist = List_new();

  // Should not be able to delete from an empty list
  assert(!List_delete(mylist, 0));
  printf(".");
  assert(!List_delete(mylist, -1));
  printf(".");
  assert(!List_delete(mylist, 1));
  printf(".");

  List_free(&mylist);

  // Test deletions on a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  // Should not be able to delete outside boundaries
  assert(!List_delete(mylist, -1));
  printf(".");
  assert(!List_delete(mylist, 5));
  printf(".");

  // Should delete on top
  assert(List_delete(mylist, 0));
  printf(".");
  assert(mylist->length == 4);
  printf(".");

  // Should delete on bottom
  assert(List_delete(mylist, 3));
  printf(".");
  assert(mylist->length == 3);
  printf(".");

  assert(strncmp(List_first(mylist), "Two", 3) == 0);
  printf(".");
  assert(strncmp(List_last(mylist), "Four", 4) == 0);
  printf(".");

  // Should delete in the middle
  assert(List_delete(mylist, 1));
  printf(".");
  assert(mylist->length == 2);
  printf(".");

  List_free(&mylist);
}

// Test update
void TestList_update() {
  // Test updates on an empty list
  List *mylist = List_new();
  char *data = NULL;

  // Should not be able to update an empty list (all out of boundaries)
  data = "Should not update";
  assert(!List_update(mylist, data, strlen(data), -1));
  printf(".");
  assert(!List_update(mylist, data, strlen(data), 0));
  printf(".");
  assert(!List_update(mylist, data, strlen(data), 1));
  printf(".");

  List_free(&mylist);

  // Test updates on a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  // Should not update out of boundaries
  assert(!List_update(mylist, data, strlen(data), -1));
  printf(".");
  assert(!List_update(mylist, data, strlen(data), 5));
  printf(".");

  // Should update on top
  data = "One - updated";
  assert(List_update(mylist, data, strlen(data), 0));
  printf(".");

  // Should update on bottom
  data = "Five - updated";
  assert(List_update(mylist, data, strlen(data), 4));
  printf(".");

  // Should update in the middle
  data = "Three - updated";
  assert(List_update(mylist, data, strlen(data), 2));
  printf(".");

  // Check that updates happened
  data = "One - updated";
  assert(strncmp(List_first(mylist), data, strlen(data)) == 0);
  printf(".");

  data = "Five - updated";
  assert(strncmp(List_last(mylist), data, strlen(data)) == 0);
  printf(".");

  data = "Three - updated";
  assert(strncmp(List_item(mylist, 2), data, strlen(data)) == 0);
  printf(".");

  List_free(&mylist);
}

// Test next
void TestList_next() {
  // Test next with an empty list
  List *mylist = List_new();

  // Next item on empty list is always null
  assert(List_next(mylist) == NULL);
  printf(".");

  List_free(&mylist);

  // Test next with a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  assert(strncmp(List_next(mylist), "One", 3) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Two", 3) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Three", 5) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Four", 4) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Five", 4) == 0);
  printf(".");

  // Navigating beyond limit should return null
  assert(List_next(mylist) == NULL);
  printf(".");
  List_free(&mylist);
}

// Test prev
void TestList_prev() {
  // Test prev with an empty list
  List *mylist = List_new();

  // Next item on empty list is always null
  assert(List_prev(mylist) == NULL);
  printf(".");

  List_free(&mylist);

  // Test prev with a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  // Going forwards and then backwards
  assert(strncmp(List_next(mylist), "One", 3) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Two", 3) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Three", 5) == 0);
  printf(".");
  assert(strncmp(List_prev(mylist), "Four", 4) == 0);
  printf(".");
  assert(strncmp(List_prev(mylist), "Three", 5) == 0);
  printf(".");
  assert(strncmp(List_prev(mylist), "Two", 3) == 0);
  printf(".");
  assert(strncmp(List_prev(mylist), "One", 3) == 0);
  printf(".");

  // Navigating beyond limit should return null
  assert(List_prev(mylist) == NULL);
  printf(".");

  List_free(&mylist);

  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  // Going backwards
  assert(strncmp(List_prev(mylist), "One", 3) == 0);
  printf(".");

  // Navigating beyond limit should return null
  assert(List_prev(mylist) == NULL);
  printf(".");

  List_free(&mylist);
}

// Test first
void TestList_first() {
  // Test with an empty list
  List *mylist = List_new();

  // First item in an empty list is always null
  assert(List_first(mylist) == NULL);
  printf(".");

  List_free(&mylist);

  // Test with a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  assert(strncmp(List_first(mylist), "One", 3) == 0);
  printf(".");

  List_free(&mylist);
}

// Test last
void TestList_last() {
  // Test with an empty list
  List *mylist = List_new();

  // Last item in an empty list is always null
  assert(List_last(mylist) == NULL);
  printf(".");

  List_free(&mylist);

  // Test with a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  assert(strncmp(List_last(mylist), "Five", 4) == 0);
  printf(".");

  List_free(&mylist);
}

// Test item
void TestList_item() {
  // Test with an empty list
  List *mylist = List_new();

  // An empty list has no items
  assert(List_item(mylist, -1) == NULL);
  printf(".");
  assert(List_item(mylist, 0) == NULL);
  printf(".");
  assert(List_item(mylist, 1) == NULL);
  printf(".");

  List_free(&mylist);

  // Test with a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  // Try to retrieve beyond boundaries results in null
  assert(List_item(mylist, -1) == NULL);
  printf(".");
  assert(List_item(mylist, 5) == NULL);
  printf(".");

  // Should be able to retrieve existing items
  assert(strncmp(List_item(mylist, 0), "One", 3) == 0);
  printf(".");
  assert(strncmp(List_item(mylist, 1), "Two", 3) == 0);
  printf(".");
  assert(strncmp(List_item(mylist, 2), "Three", 5) == 0);
  printf(".");
  assert(strncmp(List_item(mylist, 3), "Four", 4) == 0);
  printf(".");
  assert(strncmp(List_item(mylist, 4), "Five", 4) == 0);
  printf(".");

  List_free(&mylist);
}

// Test rewind
void TestList_rewind() {
  // Test with an empty list
  List *mylist = List_new();

  // Cannot rewind an empty list
  assert(!List_rewind(mylist));
  printf(".");

  List_free(&mylist);

  // Test with a predefined list
  mylist = TestList_mock();
  assert(mylist->length == 5);
  printf(".");

  assert(List_rewind(mylist));
  printf(".");

  assert(strncmp(List_next(mylist), "One", 3) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Two", 3) == 0);
  printf(".");
  assert(strncmp(List_next(mylist), "Three", 5) == 0);
  printf(".");

  // Ensure that rewind brings back the list pointer to the first item
  assert(List_rewind(mylist));
  printf(".");
  assert(strncmp(List_next(mylist), "One", 3) == 0);
  printf(".");

  List_free(&mylist);
}
