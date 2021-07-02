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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "hash.h"

#ifndef HASH_SIZE
#define HASH_SIZE 128
#endif

/**
 * A HashNode is a generic struct node, with a pointer to
 * a Tuple structure and a pointer to the next HashNode
 * in the list.
 */
typedef struct _Node HashNode;
typedef struct _Node {
  HashNode *next; ///< Pointer to the next item in the list
  Tuple data; ///< Structure that contains the data for the HashNode
} HashNode;

/**
 * A Hash is a sorted set of items,
 * like a dictionary or associative array
 */
typedef struct _Hash {
  HashNode *hash[HASH_SIZE]; ///< Hash table array, each item contains a pointer to a HashNode
  int length; ///< Total length of the Hash
} Hash;

void HashNode_free(HashNode **this);

/**
 * Creates a new empty Hash and returns its pointer
 */
Hash *Hash_new() {
  Hash *this = (Hash *)calloc(sizeof(Hash), 1);
  if (this == NULL) return NULL;
  return this;
}

/**
 * Safely deletes all nodes from the given Hash
 * starting from the first node of the last row and going up
 */
void Hash_purge(Hash *this) {
  if (this->length == 0) return;
  for (size_t i = 0; i < HASH_SIZE; i++) {
    HashNode *current = this->hash[i];
    while(current != NULL) {
      // Hook the next node as first one, current gets detached
      this->hash[i] = current->next;
      current->next = NULL;
      // Free the detached node
      HashNode_free(&current);
      // See if there are other nodes in the same row
      current = this->hash[i];
    }
    // current node is null, go to next row
  }
}

/**
 * Destroys a hash and all its nodes
 */
void Hash_free(Hash **this) {
  if (this != NULL) {
    // First walk the hash and free all its nodes
    // We implicitely trust the developer that didn't mess up
    // with the length attribute of the hash
    if (!Hash_empty(*this)) Hash_purge(*this);

    // Then free the Hash itself
    // This will erase all data in memory
    memset(*this, 0, sizeof(Hash));
    // Free the pointer to which this is pointing
    // which is the actual pointer to the hash
    free(*this);
    *this = NULL;
  }
}

/**
 * Tells if a hash is empty
 */
bool Hash_empty(const Hash *this) {
  return (this->length == 0);
}

/**
 * Returns the length of the given hash
 */
int  Hash_length(const Hash *this) {
  return this->length;
}

/**
 * Creates a new Hash node with the provided key/value/length
 */
HashNode *HashNode_new(const char *key, const void *value, size_t length) {
  HashNode *this = (HashNode *)calloc(sizeof(HashNode), 1);
  if (this == NULL) return NULL;
  // Copy the key as string
  this->data.key = strdup(key);
  if (this->data.key == NULL) {
    HashNode_free(&this);
    return NULL;
  }
  // Allocate memory for the value
  this->data.value = calloc(length, 1);
  if (this->data.value == NULL) {
    memset(this->data.key, 0, strlen(this->data.key) + 1);
    free(this->data.key);
    this->data.key = NULL;
    HashNode_free(&this);
    return NULL;
  }
  this->data.length = length;
  // Copy the actual data for the value
  memcpy(this->data.value, value, this->data.length);
  return this;
}

/**
 * Destroys the given Hash node
 */
void HashNode_free(HashNode **this) {
  if (this != NULL) {
    // Cleanup the data memory and free the data pointers
    if ((*this)->data.key != NULL) {
      memset((*this)->data.key, 0, strlen((*this)->data.key) + 1);
      free((*this)->data.key);
    }
    if ((*this)->data.value != NULL) {
      memset((*this)->data.value, 0, (*this)->data.length);
      free((*this)->data.value);
    }
    // Clean memory for the node
    memset(*this, 0, sizeof(HashNode));
    // Free and NULLify the node pointer
    free(*this);
    *this = NULL;
  }
}

/**
 * Computes the hash index for a given key
 */
int Hash_indexFor(const char *key, size_t size) {
  int index = 0;
  size_t keyLength = strlen(key);
  for (size_t i = 0; i < keyLength; i++) {
    index += key[i];
  }
  return index % size;
}

/**
 * Sets a key/value pair in given Hash
 */
bool Hash_set(Hash *this, const char *key, const void *value, size_t length) {
  int hashIndex = Hash_indexFor(key, HASH_SIZE);
  HashNode *node = this->hash[hashIndex];
  HashNode *prev = NULL;
  while (node != NULL) {
    char *currentKey = node->data.key;
    int comp = strcmp(key, currentKey);
    if (comp == 0) {
      // Update existing value
      // Create a new node and replace the current with the new one
      HashNode *item = HashNode_new(key, value, length);
      if (item == NULL) return false;
      if (prev != NULL) {
        // Item is not the first item
        prev->next = item;
      } else {
        // Item is at the start of the chain
        this->hash[hashIndex] = item;
      }
      item->next = node->next;
      node->next = NULL;
      HashNode_free(&node);
      return true;
    }
    prev = node;
    node = node->next;
  }
  // The new item is appended at the start or end of the list
  HashNode *item = HashNode_new(key, value, length);
  if (item == NULL) return false;
  if (prev == NULL) {
    this->hash[hashIndex] = item; // The list is empty, add as first item
    // ^ NOTE: we can't use 'node' here because it's a copy pointer
    // we need to insert directly into the Hash table
  } else {
    prev->next = item; // No item > new append at the end of the list
  }
  this->length += 1;
  return true;
}

/**
 * Gets the item for the given key, or NULL if the key does not exist
 */
Tuple *Hash_get(const Hash *this, const char *key) {
  if (this->length > 0) {
    int hashIndex = Hash_indexFor(key, HASH_SIZE);
    HashNode *node = this->hash[hashIndex];
    while (node != NULL) {
      int comp = strcmp(key, node->data.key);
      if (comp == 0) {
        Tuple *data = malloc(sizeof(Tuple));
        memcpy(data, &(node->data), sizeof(Tuple));
        return data;
      }
      node = node->next;
    }
  }
  // No key was found
  return NULL;
}

/**
 * Gets the value for the given key, or NULL if the key does not exist
 */
void *Hash_getValue(const Hash *this, const char *key) {
  if (this->length > 0) {
    int hashIndex = Hash_indexFor(key, HASH_SIZE);
    HashNode *node = this->hash[hashIndex];
    while (node != NULL) {
      int comp = strcmp(key, node->data.key);
      if (comp == 0) return node->data.value;
      node = node->next;
    }
  }
  // No key was found
  return NULL;
}

/**
 * Deletes the item for the given key
 * Returns true if the item did exist and was deleted successfully,
 * otherwise returns false
 */
bool Hash_delete(Hash *this, const char *key) {
  if (this->length > 0) {
    int hashIndex = Hash_indexFor(key, HASH_SIZE);
    HashNode *node = this->hash[hashIndex];
    HashNode *prev = NULL;
    while (node != NULL) {
      int comp = strcmp(key, node->data.key);
      if (comp == 0) {
        // Detach the node
        if (prev == NULL) {
          // First of the list
          this->hash[hashIndex] = node->next; // can be NULL
        } else {
          // Mid or end of the list
          prev->next = node->next; // can be NULL
          node->next = NULL;
        }
        HashNode_free(&node);
        this->length -= 1;
        return true;
      }
      prev = node;
      node = node->next;
    }
  }
  return false;
}

/**
 * Gets the key/value pair for the first Hash item
 */
Tuple *Hash_first(const Hash *this) {
  if (this->length > 0) {
    for (size_t i = 0; i < HASH_SIZE; i++) {
      HashNode *node = this->hash[i];
      if (node != NULL) {
        // Warning: 1) this just create space for the Tuple itself,
        // not for the actual content
        // 2) you will need to free the Tuple after use, but the content
        // will not be freed until the node is freed
        Tuple *data = malloc(sizeof(Tuple));
        memcpy(data, &(node->data), sizeof(Tuple));
        return data;
      }
    }
  }
  // The Hash is empty
  return NULL;
}

/**
 * Gets the key/value pair for the last Hash item
 */
Tuple *Hash_last(const Hash *this) {
  if (this->length > 0) {
    for (int i = (HASH_SIZE - 1); i >= 0; i--) {
      HashNode *node = this->hash[i];
      if (node != NULL) {
        // Walk the list until the last item
        while (node->next != NULL) {
          node = node->next;
        }
        // Warning: 1) this just create space for the Tuple itself,
        // not for the actual content
        // 2) you will need to free the Tuple after use, but the content
        // will not be freed until the node is freed
        Tuple *data = malloc(sizeof(Tuple));
        memcpy(data, &(node->data), sizeof(Tuple));
        return data;
      }
    }
  }
  // The Hash was empty
  return NULL;
}

/**
 * Destroys the given Tuple
 * Only the container, without destroying the associated data
 * Use HashNode_free() to destroy the data
 */
void Tuple_free(Tuple **this) {
  if (this != NULL) {
    // Clean memory for the Tuple
    memset(*this, 0, sizeof(Tuple));
    // Free and NULLify the Tuple pointer
    free(*this);
    *this = NULL;
  }
}

