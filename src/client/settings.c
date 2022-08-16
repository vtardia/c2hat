#include "settings.h"

#include <getopt.h>
#include <libgen.h> // for basename()
#include <stdlib.h>
#include <string.h>
#include "fsutil/fsutil.h"

static const char *kC2HatClientVersion = "1.0";
static const char *kDefaultCACertFilePath = ".local/share/c2hat/ssl/cacert.pem";
static const char *kDefaultCACertDirPath = ".local/share/c2hat/ssl";

/**
 * Displays program version
 */
void version(const char *program) {
  fprintf(
    stderr, "%1$s - C2Hat client [version %2$s]\n",
    basename((char *)program), kC2HatClientVersion
  );
}

/**
 * Displays program usage
 */
void usage(const char *program) {
  fprintf(stderr,
    "Usage: %1$s [options] <host> <port>\n"
    "       %1$s [-u YourNickname] <host> <port>\n"
    "\n"
    "For a listing of options, use %1$s --help."
    "\n", basename((char *)program)
  );
}

/**
 * Displays program help
 */
void help(const char *program) {
  fprintf(stderr,
    "%1$s - commandline C2Hat client [version %2$s]\n"
    "\n"
    "Usage: %1$s [options] <host> <port>\n"
    "       %1$s [-u YourNickname] <host> <port>\n"
    "\n"
    "%1$s is a commandline ncurses-based client for the C2Hat server\n"
    "platform.\n"
    "\n"
    "It provides an interactive chat environment to send and receive\n"
    "messages up to 280 Unicode characters, including emojis.\n"
    "\n"
    "Examples:\n"
    "\n"
    "   $ %1$s chat.example.com 10000\n"
    "   $ %1$s -u Uncl3Ozzy chat.example.com 10000\n"
    "\n"
    "Current options include:\n"
    "   -u, --user      specify a user's nickname before connecting;\n"
    "       --cacert    specify a CA certificate to verify with;\n"
    "       --capath    specify a directory where trusted CA certificates\n"
    "                   are stored; if neither cacert and capath are\n"
    "                   specified, the default path will be used:\n"
    "                   $HOME/.local/share/c2hat/ssl\n"
    "   -v, --version   display the current program version;\n"
    "   -h, --help      display this help message;\n"
    "       --debug     enable verbose logging;\n"
    "\n", basename((char *)program), kC2HatClientVersion
  );
}

/**
 * Parses command line options
 * @param[in] argc The number of arguments
 * @param[in] argv The array of arguments
 * @param[in] options Pointer to a configuration structure
 */
void parseOptions(int argc, ARGV argv, ClientOptions *params) {
  // Check that we have the minimum required command line args
  if (argc < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // Build the options list
  int debug = 0;
  struct option options[] = {
    {"user", required_argument, NULL, 'u'},
    {"cacert", required_argument, NULL, 'f'},
    {"capath", required_argument, NULL, 'd'},
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"debug", no_argument, &debug, 1},
    { NULL, 0, NULL, 0}
  };

  // Setup default SSL config
  snprintf(
    params->caCertFilePath, kMaxPath - 1,
    "%s/%s", getenv("HOME"), kDefaultCACertFilePath
  );
  snprintf(
    params->caCertDirPath, kMaxPath - 1,
    "%s/%s",getenv("HOME"), kDefaultCACertDirPath
  );

  // Setup log directory
  snprintf(
    params->logDirPath, sizeof(params->logDirPath),
    "%s/.local/state/%s", getenv("HOME"), APPNAME
  );
  if (!TouchDir(params->logDirPath, 0700)) {
    fprintf(stderr, "Unable to set the log directory: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  }

  // Parse the command line arguments into options
  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "u:hv", options, NULL);
    if( (signed char)ch == -1 ) break; // No more options available
    switch (ch) {
      case 'h': // User requested help, display it and exit
        help(argv[0]);
        exit(EXIT_SUCCESS);
      break;
      case 'v': // User requested version, display it and exit
        version(argv[0]);
        exit(EXIT_SUCCESS);
      break;
      case 'u': // User passed a nickname
        strncpy(params->user, optarg, kMaxNicknameSize - 1);
      break;
      case 'f': // User passed a CA certificate file
        strncpy(params->caCertFilePath, optarg, kMaxPath - 1);
      break;
      case 'd': // User passed a CA directory path
        strncpy(params->caCertDirPath, optarg, kMaxPath - 1);
      break;
      case 0:
        if (debug) params->logLevel = LOG_DEBUG;
      break;
    }
  }

  // We need at least 2 arguments left: host and port
  if ((argc - optind) < 2) {
    usage(argv[0]);
    exit(EXIT_FAILURE);
  }

  // Copy host and port into the options struct
  strncpy(params->host, argv[optind++], kMaxHostnameSize - 1);
  strncpy(params->port, argv[optind], kMaxPortSize - 1);
}
