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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <ctype.h>
#include <unistd.h>
#include "ini/ini.h"
#include "../c2hat.h"

#define StringEquals(op, val) (strcmp(op, val) == 0)

static const char * kProgramVersion = "1.0.0";

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
// static int handler(
//   void* data,
//   const char* section,
//   const char* name,
//   const char* value
// ) {
//   ServerConfigInfo* settings = data;
//   #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
//   if (MATCH("", "locale")) {
//     memcpy(settings->locale, value, sizeof(settings->locale) -1);
//   } else if (MATCH("log", "level")) {
//     LOG_LEVEL(settings->logLevel, value);
//   } else if (MATCH("log", "path")) {
//     memcpy(settings->logFilePath, value, sizeof(settings->logFilePath) -1);
//   } else if (MATCH("server", "host")) {
//     memcpy(settings->host, value, sizeof(settings->host) -1);
//   } else if (MATCH("server", "port")) {
//     settings->port = atoi(value);
//   } else if (MATCH("server", "max_connections")) {
//     settings->maxConnections = atoi(value);
//   } else if (MATCH("server", "pid_file_path")) {
//     memcpy(settings->pidFilePath, value, sizeof(settings->pidFilePath) -1);
//   } else if (MATCH("tls", "cert_file")) {
//     memcpy(settings->sslCertFilePath, value, sizeof(settings->sslCertFilePath) -1);
//   } else if (MATCH("tls", "key_file")) {
//     memcpy(settings->sslKeyFilePath, value, sizeof(settings->sslKeyFilePath));
//   } else {
//     return 0; // unknown section/name or error
//   }
//   return 1; // success
// }

/// Displays program version
static int usage(const char *prog, const int ret) {
  fprintf(
    stderr,
    "Usage: %s [list | [view|preview|add|edit|delete] <username>]\n",
    basename((char *)prog)
  );
  return ret;
}

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

  char configFilePath[kMaxPath] = {};
  GetConfigFilePath(configFilePath, sizeof(configFilePath));
  if (strlen(configFilePath) <= 0) {
    fprintf(stderr, "No configuration file available.\n");
    return EXIT_FAILURE;
  }
  printf("Using %s\n", configFilePath);

  // int result = ini_parse(configFilePath, handler, settings);
  // if (result == -1) {
  //   // file open error
  //   fprintf(stderr, "unable to open file '%s' - %s\n", configFilePath, strerror(errno));
  //   return false;
  // }
  // if (result == -2) {
  //   // memory allocation error
  //   fprintf(stderr, "memory allocation error - %s\n", strerror(errno));
  //   return false;
  // }
  // if (result > 0) {
  //   // parse error at line result
  //   fprintf(stderr, "parse error in %s at line %d\n", configFilePath, result);
  //   return false;
  // }

  // const char *dbFilePath = NULL; // Always from config
  // const char *operation = argv[1];

  return EXIT_SUCCESS;
}
