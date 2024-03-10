/**
 * Copyright (C) 2023 Vito Tardia <https://vito.tardia.me>
 *
 * This file is part of SQLite3Passwd
 *
 * SQLite3Passwd is a "clone" of Apache's htpasswd with a SQLite 3 backend.
 *
 * SQLite3Passwd is free software; you can redistribute it and/or
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

#include "sl3auth.h"

#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

typedef unsigned char byte;

/**
 * Converts a binary hash to readable hex text
 * @hash   The byte array hash
 * @length The length of the hash without NULL terminator
 *         (e.g. 32 for SHA256, 64 for SHA512)
 * 
 * Returns a char pointer that needs to be freed
 */
static char *Hash2Text(
  const unsigned char *hash,
  size_t length, HashAlgo algo
) {
  // Each byte is represented by 2 characters,
  // so the resulting text must have double size plus
  // the NULL terminator
  size_t resultLength = (length * 2);

  // Adding prefix as per crypt() definition
  // See https://en.wikipedia.org/wiki/Crypt_(C)
  char *prefix = (algo == SHA512Hash) ? "$6$" : "$5$";
  size_t prefixLength = strlen(prefix);

  char *text = calloc(1, prefixLength + resultLength + 1);
  if (text == NULL) return NULL;

  snprintf(text, prefixLength + 1, "%s", prefix);
  char *p = text + prefixLength;
  char *end = p + resultLength;
  for(size_t i = 0; i < length; i++) {
    if (p >= end) break; // prevents overflow
    sprintf(p, "%02x", hash[i]);
    p += 2;
  }
  return text;
}

/**
 * Encrypts the input data into a SHA256 hash and returns a text
 * representation prefixed by $5$
 */
static char *SHA256Crypt(const void *data, size_t size) {
  unsigned char hash[SHA256_DIGEST_LENGTH] = {};
  if (!EVP_Q_digest(NULL, "SHA256", NULL, data, size, hash, NULL)) {
    return NULL;
  }
  return Hash2Text(hash, sizeof(hash), SHA256Hash);
}

/**
 * Encrypts the input data into a SHA512 hash and returns a text
 * representation prefixed by $6$
 */
static char *SHA512Crypt(const void *data, size_t size) {
  unsigned char hash[SHA512_DIGEST_LENGTH] = {};
  if (!EVP_Q_digest(NULL, "SHA512", NULL, data, size, hash, NULL)) {
    return NULL;
  }
  return Hash2Text(hash, sizeof(hash), SHA512Hash);
}

/// Initialises a newly created database
static bool db_init(sqlite3 *db) {
  char sql[1024] = {};
  strcat(sql, "drop table if exists users;\n");
  strcat(sql, "create table users (\n");
  strcat(sql, "  id integer not null primary key autoincrement,\n");
  strcat(sql, "  username varchar(50) not null check(length(username) <= 50),\n");
  strcat(sql, "  password varchar(131) not null check(length(password) <= 131),\n");
  strcat(sql, "  created_at datetime not null default CURRENT_TIMESTAMP,\n");
  strcat(sql, "  updated_at datetime not null default CURRENT_TIMESTAMP\n");
  strcat(sql, ");\n");
  strcat(sql, "create unique index user_username on users (username);\n");
  strcat(sql, "create index user_created_at on users (created_at);\n");
  strcat(sql, "create index user_updated_at on users (updated_at);\n");

  char *zErrMsg = NULL;
  int rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[sl3auth] Unable to initialise users database: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    return false;
  }
  return true;
}

static int viewUserWrapper(void *userData, int colCount, char **columns, char **colNames) {
  ListUsersCallback viewUserCallback = (ListUsersCallback)userData;
  return viewUserCallback(colCount, columns, colNames);
}


sqlite3 *SL3Auth_open(const char *dbFilePath) {
  sqlite3 *db;
  bool initRequired = (access(dbFilePath, F_OK) != 0);

  // Open a connection to a database
  if (sqlite3_open(dbFilePath, &db) != SQLITE_OK) {
    fprintf(
      stderr,
      "[sl3auth] Unable to open database '%s': %s\n",
      dbFilePath,
      sqlite3_errmsg(db)
    );
    goto fail;
  }

  // Initialises the db file if newly created
  if (initRequired && !db_init(db)) goto fail;

  return db;

fail:
  sqlite3_close(db);
  return NULL;
}

char *SL3Auth_getPassword(const char *prompt) {
  char *pass = getpass(prompt != NULL ? prompt : "Enter password: ");
  char *password = strdup(pass);
  free(pass);
  return password;
}


char *SL3Auth_hashPassword(const void *data, size_t size, HashAlgo algo) {
  if (algo == SHA512Hash) return SHA512Crypt(data, size);
  if (algo == SHA256Hash) return SHA256Crypt(data, size);
  return NULL;
}

bool SL3Auth_listUsers(sqlite3 *db, ListUsersCallback viewUserCallback) {
  if (viewUserCallback == NULL || db == NULL) return false;

  char sql[] = "select * from users;";
  char *zErrMsg = NULL;

  /**
   * sqlite3_exec() Executes the query and gives control to the callback
   * passed ad 3rd argument.
   * If the callback function is not NULL, then it is invoked for each result row
   * coming out of the evaluated SQL statements.
   * The 4th argument is relayed through to the 1st argument of each callback invocation.
   * In this case we are using an internal callback and we inject the callback
   * provided by the caller using the 4th parameter.
   */
  int rc = sqlite3_exec(db, sql, viewUserWrapper, viewUserCallback, &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[sl3auth] Unable to list users: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    return false;
  }
  return true;
}

bool SL3Auth_showUser(const char *username, sqlite3 *db, ListUsersCallback callback) {
  if (callback == NULL || username == NULL || db == NULL) return false;

  sqlite3_stmt *stmt = NULL;

  char sql[] = "select * from users where username = :username;";
  char *sqlExpanded = NULL;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/show] Unable to prepare query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/show] Unable to bind username: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  // Retrieve the SQL string with the expanded parameters
  sqlExpanded = sqlite3_expanded_sql(stmt);
  if (sqlExpanded == NULL) {
    fprintf(stderr, "[sl3auth/show] Unable to get expanded SQL: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  char *zErrMsg = NULL;
  int rc = sqlite3_exec(db, sqlExpanded, viewUserWrapper, callback, &zErrMsg);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/show] Unable to run query: %s\n", zErrMsg);
    sqlite3_free(zErrMsg);
    goto fail;
  }
  sqlite3_free(sqlExpanded);
  sqlite3_finalize(stmt);
  return true;
fail:
  sqlite3_free(sqlExpanded);
  sqlite3_finalize(stmt);
  return false;
}

bool SL3Auth_upsertUser(const char *username, const char *password, HashAlgo algo, sqlite3 *db) {
  if (username == NULL || password == NULL || db == NULL) return false;

  char sql[] = "insert into users (username, password) values (:username, :password) ON CONFLICT(username) DO UPDATE SET password = :password;";
  sqlite3_stmt *stmt = NULL;

  char *hash = SL3Auth_hashPassword(password, strlen(password), algo);
  if (hash == NULL) {
    fprintf(stderr, "[sl3auth/upsert]: Unable to hash password\n");
    goto fail;
  }
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/upsert] Unable to prepare query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/upsert] Unable to bind username: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_bind_text(stmt, 2, hash, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/upsert] Unable to bind password: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "[sl3auth/upsert] Unable to run query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_finalize(stmt) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/upsert] Unable to finalise query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  free(hash);
  return true;
fail:
  free(hash);
  sqlite3_finalize(stmt);
  return false;
}

bool SL3Auth_deleteUser(const char *username, sqlite3 *db) {
  if (username == NULL || db == NULL) return false;

  char sql[] = "delete from users where username = :username;";
  sqlite3_stmt *stmt = NULL;

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/delete] Unable to prepare query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/delete] Unable to bind username: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    fprintf(stderr, "[sl3auth/delete] Unable to run query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_finalize(stmt) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/delete] Unable to finalise query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  return true;
fail:
  sqlite3_finalize(stmt);
  return false;
}

bool SL3Auth_verifyUser(const char *username, const char *password, sqlite3 *db) {
  if (username == NULL || password == NULL || db == NULL) return false;

  sqlite3_stmt *stmt = NULL;
  char *storedPassword = NULL;

  char sql[] = "select password from users where username = :username;";

  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/verify] Unable to prepare query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_bind_text(stmt, 1, username, -1, SQLITE_STATIC) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/verify] Unable to bind username: %s\n", sqlite3_errmsg(db));
    goto fail;
  }
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    goto fail;
  }

  // Extract the stored password
  const unsigned char *value = sqlite3_column_text(stmt, 0);
  if (value != NULL) storedPassword = strdup((char *)value);
  if (storedPassword == NULL) {
    fprintf(stderr, "[sl3auth/verify]: Unable to verify password\n");
    goto fail;
  }

  if (sqlite3_finalize(stmt) != SQLITE_OK) {
    fprintf(stderr, "[sl3auth/verify] Unable to finalise query: %s\n", sqlite3_errmsg(db));
    goto fail;
  }

  // Hash the user-provided pasword using the correct algorhythm
  HashAlgo algo = (strncmp(storedPassword, "$6$", 3) == 0) ? SHA512Hash : SHA256Hash;
  char *hash = SL3Auth_hashPassword(password, strlen(password), algo);
  if (hash == NULL) {
    fprintf(stderr, "[sl3auth/verify]: Unable to hash the provided password\n");
    goto fail;
  }

  // Compare the two hashes
  bool success = (strcmp(storedPassword, hash) == 0);
  free(hash);
  free(storedPassword);
  return success;

fail:
  sqlite3_finalize(stmt);
  return false;
}
