/**
 * Copyright (C) 2023 Vito Tardia <https://vito.tardia.me>
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

#ifndef SL3_AUTH_H
#define SL3_AUTH_H

  #include <sqlite3.h>
  #include <stdbool.h>
  #include <stdlib.h>

  typedef enum {
    SHA256Hash = 5,
    SHA512Hash = 6
  } HashAlgo;

  typedef int(*ListUsersCallback)(int, char**, char**);

  /**
   * Creates a handle for the given database file
   * The file is created and initialised if does not exists
   * Returns NULL on error
   */
  sqlite3 *SL3Auth_open(const char *dbFilePath);

  /**
   * Asks for the password via terminal and returns
   * a string that will need to be freed after use
   */
  char *SL3Auth_getPassword(const char *prompt);

  /**
   * Hashes the provided password with the chosen algorithm (see HashAlgo enum)
   */
  char *SL3Auth_hashPassword(const void *data, size_t size, HashAlgo algo);

  /**
   * Inserts or updated the given username and password credentials
   * within the provided database file
   */
  bool SL3Auth_upsertUser(const char *username, const char *password, HashAlgo algo, sqlite3 *db);

  #define SL3Auth_addUser(username, password, algo, db) SL3Auth_upsertUser(username, password, algo, db)
  #define SL3Auth_updateUser(username, password, algo, db) SL3Auth_upsertUser(username, password, algo, db)

  /**
   * Fetches a list of users and displays them using the provided callback
   * 
   * The callback is called once for each row, with the following arguments
   * 
   * @param int columnCount
   * @param char **columns
   * @param char **columnNames
   * 
   * If the callback returns non-zero, SL3Auth_listUsers will return false
   * and will stop processing the remaining rows.
   */
  bool SL3Auth_listUsers(sqlite3 *db, ListUsersCallback);

  /**
   * Deletes the given username from the database
   */
  bool SL3Auth_deleteUser(const char *username, sqlite3 *db);

  /**
   * Displays the details about a user from the database using the provided callback
   */
  bool SL3Auth_showUser(const char *username, sqlite3 *db, ListUsersCallback callback);

  /**
   * Authenticates the giver username/pasword credentials
   */
  bool SL3Auth_verifyUser(const char *username, const char *password, sqlite3 *db);
#endif
