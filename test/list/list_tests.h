/*
 * Copyright (C) 2020 Vito Tardia
 */

#ifndef LIST_TESTS_H
#define LIST_TESTS_H

// Test new, free, empty
void TestList_new();

// Creates a test list for all the other functions
List * TestList_mock();

// Display a list on stdout
void TestList_display(List*);

// Test search
void TestList_search();

// Test sort
void TestList_sort();

// Test append
void TestList_append();

// Test prepend
void TestList_prepend();

// Test insert
void TestList_insert();

// Test delete
void TestList_delete();

// Test update
void TestList_update();

// Test next
void TestList_next();

// Test prev
void TestList_prev();

// Test first
void TestList_first();

// Test last
void TestList_last();

// Test item
void TestList_item();

// Test rewind
void TestList_rewind();

#endif
