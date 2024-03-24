/**
 * Copyright (C) 2023 Vito Tardia <https://vito.tardia.me>
 *
 * This file is an adaptation of SQLite3Passwd for C2Hat
 *
 * SQLite3Passwd is a "clone" of Apache's htpasswd with a SQLite 3 backend.
 *
 * C2Hat is a simple client/server TCP chat written in C
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
 * License along with SQLite3Passwd; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA
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
#include <string.h>
#include <libgen.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include "ini/ini.h"
#include "validate/validate.h"
#include "../c2hat.h"
#include "sl3auth/sl3auth.h"

static const char * kProgramVersion = "1.0.0";

/// Regex pattern used to validate the user nickname
static const char *kRegexNicknamePattern = "^[[:alpha:]][[:alnum:]!@#$%&]\\{1,14\\}$";

/// Validation error message for invalid user names, includes the rules
static const char *kErrorMessageInvalidUsername = "Usernames must start with a letter and contain 2-15 latin characters and !@#$%&";

// @todo Keep the above definitions in sync with server.c or move them to a common file.

#define OP_LIST    "list"
#define OP_VIEW    "view"
#define OP_PREVIEW "preview"
#define OP_ADD     "add"
#define OP_EDIT    "edit"
#define OP_DELETE  "delete"
#define OP_VERIFY  "verify"

#define StringEquals(op, val) (strcmp(op, val) == 0)

#define CheckUsername() if (NULL == username) goto failWithUsage;

/// Converts raw column names to pretty labels
static char *labelFor(const char *col) {
  if (StringEquals(col, "id")) return "ID";
  if (StringEquals(col, "username")) return "Username";
  if (StringEquals(col, "password")) return "Password";
  if (StringEquals(col, "created_at")) return "Created";
  if (StringEquals(col, "updated_at")) return "Updated";
  return "(n/a)";
}

/// Callback used to display a user's details
static int showUser(int columnCount, char **columns, char **columnNames) {
  printf("\n");
  for(int i = 0; i < columnCount; i++) {
    printf("%10s: %s\n", labelFor(columnNames[i]), columns[i] ? columns[i] : "NULL");
  }
  printf("\n");
  return EXIT_SUCCESS;
}

/**
 * Gets the path of the configuration file to load
 * (duplicate from server/settings.c)
 *
 * TODO Extract settings as a common library
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
static char *GetConfigFilePath(char *filePath, size_t length) {
  /* $HOME/.config/<app>/server.conf */
  char userConfigFilePath[kMaxPath] = {};
  snprintf(
    userConfigFilePath, sizeof(userConfigFilePath),
    "%s/.config/%s/server.conf", getenv("HOME"), APPNAME
  );

  /* /etc/<app>/server.conf */
  char etcConfigFilePath[kMaxPath] = {};
  snprintf(
    etcConfigFilePath, sizeof(etcConfigFilePath),
    "/etc/%s/server.conf", APPNAME
  );

  /* /usr/local/etc/<app>/server.conf */
  char usrLocalEtcConfigFilePath[kMaxPath] = {};
  snprintf(
    usrLocalEtcConfigFilePath, sizeof(usrLocalEtcConfigFilePath),
    "/usr/local/etc/%s/server.conf", APPNAME
  );

  // Clean the output buffer first
  memset(filePath, 0, length);

  if (getuid() != 0 && access(userConfigFilePath, R_OK) == 0) {
    // User is not root and has a config file under their home dir
    memcpy(filePath, userConfigFilePath, length);
  } else if (access(etcConfigFilePath, R_OK) == 0) {
    // User is root or there is no personal config file,
    // lookup global files
    memcpy(filePath, etcConfigFilePath, length);
  } else if (access(usrLocalEtcConfigFilePath, R_OK) == 0) {
    memcpy(filePath, usrLocalEtcConfigFilePath, length);
  }
  // If none of the files exist an empty string is returned
  return filePath;
}

/**
 * Handles INI key/value pairs
 *
 * For every tuple returns 1 on success, 0 on failure
 * @param[in] data    Pointer to a user data type to store results
 * @param[in] section Section for the current tuple, or empty string
 * @param[in] name    Name of the setting
 * @param[in] value   Value of the setting
 */
static int handler(
  void* data,
  const char* section,
  const char* name,
  const char* value
) {
  char* usersDbFilePath = data;
  #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
  if (MATCH("auth", "users_file")) {
    memcpy(usersDbFilePath, value, kMaxPath);
  }
  // We don't care about other sections and if users file doesn't match
  // we use the default value
  return 1;
}

/**
 * Returns the path for the users database
 * (duplicate from server/settings.c)
 *
 * TODO Extract settings as a common library
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
char *GetDefaultUsersFilePath(char *filePath, size_t length) {
  memset(filePath, 0, length); // Reset first
  if (getuid() == 0) {
    // Running as root, use system directories
    snprintf(filePath, length, "/usr/local/%s/users.db", APPNAME);
  } else {
    // Running as local user, use local directory
    snprintf(filePath, length, "%s/.local/state/%s/users.db", getenv("HOME"), APPNAME);
  }
  return filePath;
}

/**
 * Validates a username with a pre-defined regex
 * @param[in]  username
 * @param[out] success/failure
 */
bool UsernameIsValid(const char *username) {
  char error[512] = {};
  int valid = Regex_match(username, kRegexNicknamePattern, error, sizeof(error));
  if (valid) return true;
  if (valid < 0) {
    // There has been an error in either compiling
    // or executing the regex above, we log it for investigation
    fprintf(stderr, "Unable to validate username '%s': %s\n", username, error);
  }
  return false;
}

/// Displays program usage
static int usage(const char *prog, const int ret) {
  fprintf(
    stderr,
    "Usage: %s [-c /path/to/config.conf] [list | [view|preview|add|edit|delete] <username>]\n",
    basename((char *)prog)
  );
  return ret;
}

/// Displays program version
static int version(const char *prog) {
  fprintf(
    stdout,
    "%s %s\n",
    basename((char *)prog), kProgramVersion
  );
  return EXIT_SUCCESS;
}

/// Displays program help
static int help(const char *prog) {
  fprintf(stderr,
    "\n%1$s - manage SQLite db for C2Hat user authentication [version %2$s]\n"
    "\n"
    "Usage: %1$s [options] <command> [<username> [options]]\n"
    "\n"
    "Available commands:\n"
    "\n"
    "   list                  Display the list of users in the database.\n"
    "   preview <username>    Dry run, compute the password for the given user\n"
    "                         without saving it.\n"
    "   view <username>       Display the details of the given user.\n"
    "   add <username>        Add a new user or update an existing user.\n"
    "   edit <username>       Update an existing user or create a new one.\n"
    "   delete <username>     Delete an existing user.\n"
    "   verify <username>     Verify a given username/password combination.\n"
    "\n"
    "Current options include:\n"
    "   -v, --version   display the current program version;\n"
    "   -h, --help      display this help message;\n"
    "       --sha512    select the SHA512 algorhythm for add/edit/preview\n"
    "                   instead of the default SHA256 (must be specified\n"
    "                   after the username);\n"
    "\n"
    "The destination database file is automatically created if does not exist.\n"
    "\n", basename((char *)prog), kProgramVersion
  );
  return EXIT_SUCCESS;
}

int main(int argc, char const *argv[]) {
  // Validate minimal arguments list and global options
  if (argc < 2) {
    return usage(argv[0], EXIT_FAILURE);
  }

  if (argc == 2) {
    if (StringEquals(argv[1], "-v") || StringEquals(argv[1], "--version")) {
      return version(argv[0]);
    }
    if (StringEquals(argv[1], "--help") || StringEquals(argv[1], "-h")) {
      return help(argv[0]);
    }
  }

  // Load default first
  char usersDbFilePath[kMaxPath] = {};
  GetDefaultUsersFilePath(usersDbFilePath, sizeof(usersDbFilePath));

  char *operation = NULL;
  char *username = NULL;
  char *enc = NULL;

  // Check if there is a custom config file
  char configFilePath[kMaxPath] = {};
  if (StringEquals(argv[1], "--config") || StringEquals(argv[1], "-c")) {
    if (NULL == argv[2]) {
      fprintf(stderr, "No configuration file provided.\n");
      return EXIT_FAILURE;
    }
    stpncpy(configFilePath, argv[2], kMaxPath);
    operation = (char*) argv[3];
    username = (char*) argv[4];
    enc = (char*) argv[5];
  } else {
    // See if there is a default config file
    GetConfigFilePath(configFilePath, sizeof(configFilePath));
    operation = (char*) argv[1];
    username = (char*) argv[2];
    enc = (char*) argv[3];
  }

  // Override with config file if exist
  if (strlen(configFilePath) > 0) {
    printf("Using config file: %s\n", configFilePath);
    int result = ini_parse(configFilePath, handler, usersDbFilePath);
    if (result == -1) {
      // file open error
      fprintf(stderr, "Unable to open file '%s' - %s\n", configFilePath, strerror(errno));
      return EXIT_FAILURE;
    }
    if (result == -2) {
      // memory allocation error
      fprintf(stderr, "Memory allocation error - %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    if (result > 0) {
      // parse error at line result
      fprintf(stderr, "Parse error in %s at line %d\n", configFilePath, result);
      return EXIT_FAILURE;
    }
  } else {
    fprintf(stderr, "No configuration file available.\n");
  }
  printf("Using DB File: %s\n", usersDbFilePath);

  // Try to open/create the database file
  sqlite3 *db = SL3Auth_open(usersDbFilePath);
  if (db == NULL) return EXIT_FAILURE;

  if (NULL == operation) goto failWithUsage;

  // Perform the selected operation
  if (StringEquals(operation, OP_LIST)) {
    if (!SL3Auth_listUsers(db, showUser)) goto fail;
  } else if (StringEquals(operation, OP_VIEW)) {
    CheckUsername();
    if (!SL3Auth_showUser(username, db, showUser)) goto fail;
  } else if (
      StringEquals(operation, OP_ADD)
      || StringEquals(operation, OP_EDIT)
      || StringEquals(operation, OP_PREVIEW)
  ) {
    CheckUsername();

    if (!UsernameIsValid(username)) {
      fprintf(stderr, "%s\n", kErrorMessageInvalidUsername);
      goto fail;
    }

    // Select algorhythm
    HashAlgo algo = SHA256Hash;
    if (enc != NULL && StringEquals(enc, "--sha512")) {
      algo = SHA512Hash;
    }

    // Ask for password
    char *password = SL3Auth_getPassword("Enter new password: ");

    // Dry run, hash password and display only
    if (StringEquals(operation, OP_PREVIEW)) {
      char *hash = SL3Auth_hashPassword(password, strlen(password), algo);
      free(password);
      if (hash == NULL) {
        fprintf(stderr, "Unable to hash password\n");
        goto fail;
      }
      printf("\nUsername: %s\nPassword: %s\n\n", username, hash);
      free(hash);
    } else {
      if (!SL3Auth_upsertUser(username, password, algo, db)) {
        free(password);
        goto fail;
      }
      free(password);
    }
  } else if (StringEquals(operation, OP_VERIFY)) {
    CheckUsername();
    // Ask for password
    char *password = SL3Auth_getPassword("Enter password: ");
    if (!SL3Auth_verifyUser(username, password, db)) {
      fprintf(stderr, "Invalid username or password\n");
      free(password);
      goto fail;
    }
    free(password);
    printf("OK\n");
  } else if (StringEquals(operation, OP_DELETE)) {
    CheckUsername();
    if (!SL3Auth_deleteUser(username, db)) goto fail;
  } else {
    goto failWithUsage;
  }

  sqlite3_close(db);
  return EXIT_SUCCESS;

failWithUsage:
  sqlite3_close(db);
  return usage(argv[0], EXIT_FAILURE);

fail:
  sqlite3_close(db);
  return EXIT_FAILURE;
}
