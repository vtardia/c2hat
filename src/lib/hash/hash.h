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

#ifndef HASHES_H
#define HASHES_H

  #include <stdbool.h>

  /**
   * A Hash is a sorted set of items,
   * like a dictionary or associative array
   */
  typedef struct _Hash Hash;

  /**
   * A Tuple contains a key/value pair
   * The key is a NULL terminated string
   * The value can be whatever
   * The length is the size of the value data
   */
  typedef struct {
    char *key;
    void *value; ///< Pointer to the actual data
    size_t length; ///< Size of the data
  } Tuple;

  /**
   * Creates a new Hash and returns a pointer to it
   */
  Hash *Hash_new();

  /**
   * Destroys a hash and all its nodes
   * The pointer to a hash pointer should passed
   * Hash * myhash = Hash_new();
   * Hash_free(&myhash);
   */
  void Hash_free(Hash **);

  /**
   * Checks if a hash is empty
   */
  bool Hash_empty(const Hash *);

  /**
   * Returns the length of the given hash
   */
  int  Hash_length(const Hash *);

  /**
   * Sets a key-value pair in the hash
   * If the key already exists, the corresponding value is updated
   */
  bool Hash_set(Hash *this, const char *key, const void *value, size_t length);

  /**
   * Gets the item for the given key, or NULL if the key does not exist
   */
  Tuple *Hash_get(const Hash *this, const char *key);

  /**
   * Get the value for the given key or NULL if the key don't exist
   */
  void *Hash_getValue(const Hash *this, const char *key);

  /**
   * Deletes the node at the corresponding key
   * Returns true if the item did exist and was deleted successfully
   * otherwise returns false
   */
  bool Hash_delete(Hash *this, const char *key);

  /**
   * Return the first element from the ordered hash as a tuple key/value
   */
  Tuple *Hash_first(const Hash *this);

  /**
   * Return the last element from the ordered hash as a tuple key/value
   */
  Tuple *Hash_last(const Hash *this);

  /**
   * Destroys the given Tuple
   * Only the container, without destroying the associated data
   * Use HashNode_free() to destroy the data
   */
  void Tuple_free(Tuple **this);
#endif

