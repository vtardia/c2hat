/**
 * Copyright (C) 2021 Vito Tardia
 *
 * This file is part of vHashLib.
 *
 * vHashLib is a simple C implementation of hashes
 * (associative arrays) using Hash Tables.
 *
 * vHashLib is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "hash/hash.h"
#include "hash_tests.h"

// Test new, free, empty
void TestHash_new() {
  Hash *myhash = Hash_new();

  assert(myhash != NULL);
  printf(".");
  assert(Hash_length(myhash) == 0);
  printf(".");
  assert(Hash_empty(myhash));
  printf(".");

  // Delete list
  // We need to pass myhash by reference (ie pointer to pointer)
  // otherwise a copy of the pointer will be freed, but the original
  // still remains
  Hash_free(&myhash);
  assert(myhash == NULL);
  printf(".");
}

// Tests both get() and set()
void TestHash_set() {
  Hash *myhash = Hash_new();

  // Add a bunch of elements and check that the length matches
  assert(Hash_set(myhash, "b", "bar", 4));
  printf(".");
  assert(Hash_set(myhash, "a", "foo", 4));
  printf(".");
  assert(Hash_set(myhash, "c", "baz", 4));
  printf(".");
  assert(Hash_length(myhash) == 3);
  printf(".");

  // Test that the values are preserved
  assert(strcmp((char*)Hash_getValue(myhash, "b"), "bar") == 0);
  printf(".");

  Tuple *b = Hash_get(myhash, "b");
  assert(strcmp(b->key, "b") == 0);
  printf(".");
  assert(strcmp((char*)b->value, "bar") == 0);
  printf(".");
  Tuple_free(&b);

  assert(strcmp((char*)Hash_getValue(myhash, "c"), "baz") == 0);
  printf(".");

  assert(strcmp((char*)Hash_getValue(myhash, "a"), "foo") == 0);
  printf(".");

  // Test that wa cannot get a non-existing element
  assert(Hash_getValue(myhash, "z") == NULL);
  printf(".");

  // Test that we can add more elements
  assert(Hash_set(myhash, "z", "zorro", 6));
  printf(".");
  assert(Hash_length(myhash) == 4);
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, "z"), "zorro") == 0);
  printf(".");

  // Test that we can override values
  char *override = "fizzbuz";
  assert(Hash_set(myhash, "b", override, strlen(override) + 1));
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, "b"), override) == 0);
  printf(".");

  Hash_free(&myhash);
}

void TestHash_first() {
  Hash *myhash = Hash_new();

  // Test that the first element of an empty hash is NULL
  Tuple *item = Hash_first(myhash);
  assert(item == NULL);
  printf(".");

  // Add some values
  assert(Hash_set(myhash, "b", "bar", 4));
  printf(".");
  assert(Hash_set(myhash, "c", "baz", 4));
  printf(".");
  assert(Hash_set(myhash, "a", "foo", 4));
  printf(".");

  // Test that the first item is picked up correctly
  // Note: we can assert the first and last if the keys are
  // single chars, otherwise it's not guaranteed
  item = Hash_first(myhash);
  assert(strcmp(item->key, "a") == 0);
  printf(".");
  Tuple_free(&item);

  // Insert a new item which shoulg go on the top of the list
  assert(Hash_set(myhash, "0", "zero", 5));
  printf(".");

  // Insert a new element that should go on the bottom
  assert(Hash_set(myhash, "z", "zed", 4));
  printf(".");

  // Test that the new first item is picked up correctly
  item = Hash_first(myhash);
  assert(strcmp(item->key, "0") == 0);
  printf(".");
  Tuple_free(&item);

  Hash_free(&myhash);
}

void TestHash_last() {
  Hash *myhash = Hash_new();

  // Test that the last element of an empty hash is NULL
  Tuple *item = Hash_last(myhash);
  assert(Hash_last(myhash) == NULL);
  printf(".");

  // Fill some values
  assert(Hash_set(myhash, "b", "bar", 4));
  printf(".");
  assert(Hash_set(myhash, "c", "baz", 4));
  printf(".");
  assert(Hash_set(myhash, "a", "foo", 4));
  printf(".");

  // Test that it picks the right last element
  item = Hash_last(myhash);
  assert(strcmp(item->key, "c") == 0);
  printf(".");
  Tuple_free(&item);

  // Append an element that should go at the end
  assert(Hash_set(myhash, "z", "zorro", 6));
  printf(".");

  // Test that the new element is the new last
  item = Hash_last(myhash);
  assert(strcmp(item->key, "z") == 0);
  printf(".");
  Tuple_free(&item);

  Hash_free(&myhash);
}

void TestHash_delete() {
  Hash *myhash = Hash_new();

  // Test that I cannot delete from an empty hash
  assert(!Hash_delete(myhash, "fool"));
  printf(".");

  // Fill some data
  assert(Hash_set(myhash, "bob", "bar", 4));
  printf(".");
  assert(Hash_set(myhash, "alice", "foo", 4));
  printf(".");
  assert(Hash_set(myhash, "chris", "baz", 4));
  printf(".");
  assert(Hash_length(myhash) == 3);
  printf(".");

  // Test that the inserted data are correct
  assert(strcmp((char*)Hash_getValue(myhash, "bob"), "bar") == 0);
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, "chris"), "baz") == 0);
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, "alice"), "foo") == 0);
  printf(".");

  // Test the deletion of one item and ensure the length is updated
  assert(Hash_delete(myhash, "bob"));
  printf(".");
  assert(Hash_length(myhash) == 2);
  printf(".");
  assert(Hash_getValue(myhash, "bob") == NULL);
  printf(".");

  // Test that I cannot delete non-existing elemente
  assert(!Hash_delete(myhash, "bob"));
  printf(".");

  Hash_free(&myhash);
}

void TestHash_unicode() {
  Hash *myhash = Hash_new();
  char *key = NULL;
  char *value = NULL;

  printf("\n");

  // Test that I can use a unicode key and normal string as value
  key = "🎃🎃";
  value = "2 pumpkins";
  assert(Hash_set(myhash, key, value, strlen(value) + 1));
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, key), value) == 0);
  printf(". ");
  printf("%s => %s\n", key, (char*)Hash_getValue(myhash, key));

  // Test that I can use a regular key and unicode string as value
  key = "Pumpkins";
  value = "🎃🎃";
  assert(Hash_set(myhash, key, value, strlen(value) + 1));
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, key), value) == 0);
  printf(". ");
  printf("%s => %s\n", key, (char*)Hash_getValue(myhash, key));

  // Test that I can use a unicode key and value
  key = "水调歌头";
  value = "🎃🎃";
  assert(Hash_set(myhash, key, value, strlen(value) + 1));
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, key), value) == 0);
  printf(". ");
  printf("%s => %s\n", key, (char*)Hash_getValue(myhash, key));

  // Same as above but expressing the key in UTF-8 bytes
  key = "\xE6\xB0\xB4\xE8\xB0\x83\xE6\xAD\x8C\xE5\xA4\xB4";
  value = "🎃🎃";
  assert(Hash_set(myhash, key, value, strlen(value) + 1));
  printf(".");
  assert(strcmp((char*)Hash_getValue(myhash, key), value) == 0);
  printf(". ");
  printf("%s => %s\n", key, (char*)Hash_getValue(myhash, key));

  Hash_free(&myhash);
}

// Loads a big list of Unicode keys from a file and
// inject them into a hash
void TestHash_bulk() {
  Hash *myhash = Hash_new();

  char *key = NULL;

  char *srcFilePath = "test/hash/utf8_1000x16xucs4.txt";
  /* char *srcFilePath = "test/hash/lorem.txt"; */

  FILE *source = fopen(srcFilePath, "r");
  assert(source != NULL);

  char buffer[1024] = {0};
  int line = 1;
  while (fscanf(source, "%s\n", buffer) != EOF) {
    key = buffer;
    assert(Hash_set(myhash, key, &line, sizeof(int)));
    assert(*((int*)Hash_getValue(myhash, key)) = line);
    printf(".");

    Tuple *t = Hash_get(myhash, key);
    assert(strcmp(t->key, key) == 0);
    printf(".");
    assert(*((int*)t->value) = line);
    printf(".");
    Tuple_free(&t);

    memset(buffer, 0, 1024);
    line++;
  }
  fclose(source);

  printf("\n");
  Hash_free(&myhash);
}

