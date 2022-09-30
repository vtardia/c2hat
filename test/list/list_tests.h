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
