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

/// @file main.c
#include "settings.h"
#include "commands.h"
#include <libgen.h> // for basename()

/// Current version
static const char *kC2HatServerVersion = "1.0";

/// Default connection limit
const int kDefaultMaxClients = 5;

/// Default server port
const int kDefaultServerPort = 10000;

/// Default server host
#define kDefaultServerHost "localhost"

/// Displays program usage
void usage(const char *program);

/**
 * Main server entry point
 */
int main(int argc, ARGV argv) {
  // At least a command is required
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  Command command = parseCommand(argc, argv[1]);

  if (kCommandStart == command) {
    // Initialise a default configuration
    ServerConfigInfo settings = {
      .foreground = false,
      .host = kDefaultServerHost,
      .port = kDefaultServerPort,
      .maxConnections = kDefaultMaxClients,
      .logLevel = LOG_INFO
    };
    if (parseOptions(argc, argv, &settings)) {
      return CMD_runStart(&settings);
    }
    usage(argv[0]);
    return EXIT_FAILURE;
  }

  if (kCommandStatus == command) return CMD_runStatus();

  if (kCommandStop == command) return CMD_runStop();

  fprintf(stderr, "Unknown command: '%s'\n", argv[1]);
  usage(argv[0]);
  return EXIT_FAILURE;
}

/**
 * Displays program usage
 */
void usage(const char *program) {
  fprintf(stderr,
    "%1$s - Free TCP Chat Server [version %2$s]\n"
    "\n"
    "Usage: %1$s <command> [options]\n"
    "       %1$s start --ssl-cert=</path/to/cert.pem> --ssk-key=</path/to/key.pem>\n"
    "                    [--foreground] [-h <host>] [-p <port>] [-m <max-clients>]\n"
    "       %1$s stop\n"
    "       %1$s status\n"
    "\n"
    "Current available commands are:\n"
    "       start          start the chat server;\n"
    "       stop           stop the chat server, if running in background;\n"
    "       status         display the chat server status and configuration;\n"
    "\n"
    "Current start options include:\n"
    "   -c, --config-file  specify the path for a custom configuration file;\n"
    "   -s, --ssl-cert     specify the path for the server TLS certificate;\n"
    "   -k, --ssl-key      specify the path for the server private key\n"
    "   -h, --host         specify the listening host name or IP (default = localhost);\n"
    "   -p, --host         specify the listening TCP port number (default = 10000);\n"
    "   -m, --max-clients  specify the maximum number of connections (default = 5);\n"
    "       --foreground   run the server in foreground;\n"
    "\n", basename((char *)program), kC2HatServerVersion);
}
