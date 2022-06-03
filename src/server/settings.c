#include "settings.h"

#include <getopt.h>
#include "ini/ini.h"

/**
 * Gets the path of the configuration file to load
 * if no custom configuration is provided from the
 * command line.
 *
 * TODO Separate Win/Mac/Linux and wrap into a macro
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
char *GetConfigFilePath(char *filePath, size_t length) {
  char userConfigFilePath[kMaxPath] = {};
  snprintf(
    userConfigFilePath, sizeof(userConfigFilePath),
    "%s/.config/c2hat/c2hat-server.conf", getenv("HOME")
  );
  memset(filePath, 0, length);
  if (getuid() != 0 && access(userConfigFilePath, R_OK) == 0) {
    // User is not root and has a config file under their home dir
    memcpy(filePath, userConfigFilePath, length);
  } else if (access("/etc/c2hat/c2hat-server.conf", R_OK) == 0) {
    // User is root or there is no personal config file,
    // lookup global files
    memcpy(filePath, "/etc/c2hat/c2hat-server.conf", length);
  } else if (access("/usr/local/etc/c2hat/c2hat-server.conf", R_OK) == 0) {
    memcpy(filePath, "/usr/local/etc/c2hat/c2hat-server.conf", length);
  }
  // If none of the files exist an empty string is returned
  return filePath;
}

/**
 * Returns the path for the PID file
 *
 * TODO write Win/Mac/Linux versions and switch using a macro
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
char *GetDefaultPidFilePath(char *filePath, size_t length) {
  if (getuid() == 0) {
    // Running as root, use system directories
    snprintf(filePath, length, "/var/run/c2hat.pid");
  } else {
    // Running as local user, use local directory
    snprintf(filePath, length, "%s/.local/run/c2hat.pid", getenv("HOME"));
  }
  return filePath;
}

/**
 * Returns the path for the Log file
 *
 * TODO write Win/Mac/Linux versions and switch using a macro
 *
 * @param[in] filePath Pointer to a char buffer
 * @param[in] length   The length of the buffer
 * @param[out]         The pointer to filePath
 */
char *GetDefaultLogFilePath(char *filePath, size_t length) {
  if (getuid() == 0) {
    // Running as root, use system path
    snprintf(filePath, length, "/var/log/c2hat-server.log");
  } else {
    // Running as local user, use local directory
    snprintf(filePath, length, "%s/.local/state/c2hat-server.log", getenv("HOME"));
  }
  return filePath;
}

/**
 * Returns the path for the TLS certificate and key file
 *
 * TODO write Win/Mac/Linux versions and switch using a macro
 *
 * @param[in] certPath     Pointer to a char buffer for the certificate
 * @param[in] certLength   The length of the certificate buffer
 * @param[in] keyPath      Pointer to a char buffer for the private key
 * @param[in] keyLength    The length of the private key buffer
 */
void GetDefaultTlsFilePaths(
  char *certPath, size_t certLength,
  char *keyPath, size_t keyLength
) {
  // Paranoid reset of output buffers
  memset(certPath, 0, certLength);
  memset(keyPath, 0, keyLength);
  char userCertPath[kMaxPath] = {};
  char userKeyPath[kMaxPath] = {};
  snprintf(
    userCertPath, sizeof(userCertPath),
    "%s/.config/c2hat/ssl/cert.pem", getenv("HOME")
  );
  snprintf(
    userKeyPath, sizeof(userKeyPath),
    "%s/.config/c2hat/ssl/key.pem", getenv("HOME")
  );
  if (getuid() != 0 && access(userCertPath, R_OK) == 0
    && access(userKeyPath, R_OK) == 0) {
      // Not running as root and we have local files
      memcpy(certPath, userCertPath, certLength);
      memcpy(keyPath, userKeyPath, keyLength);
  } else if (access("/etc/c2hat/ssl/cert.pem", R_OK) == 0
    && access("/etc/c2hat/ssl/key.pem", R_OK) == 0) {
      // Running as root or no user files available
      // Try /etc/c2hat/ssl
      snprintf(certPath, certLength, "/etc/c2hat/ssl/cert.pem");
      snprintf(keyPath, keyLength, "/etc/c2hat/ssl/key.pem");
  } else if (access("/usr/local/etc/c2hat/ssl/cert.pem", R_OK) == 0
    && access("/usr/local/etc/c2hat/ssl/key.pem", R_OK) == 0) {
      // Try /usr/local/etc/c2hat/ssl
      snprintf(certPath, certLength, "/usr/local/etc/c2hat/ssl/cert.pem");
      snprintf(keyPath, keyLength, "/usr/local/etc/c2hat/ssl/key.pem");
  }
  // File paths will be empty at this point, will beed to be
  // overridden with a custom conf file or from the command line
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
int handler(
  void* data,
  const char* section,
  const char* name,
  const char* value
) {
  ServerConfigInfo* settings = data;
  #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
  if (MATCH("", "locale")) {
    memcpy(settings->locale, value, sizeof(settings->locale) -1);
  } else if (MATCH("log", "level")) {
    LOG_LEVEL(settings->logLevel, value);
  } else if (MATCH("log", "path")) {
    memcpy(settings->logFilePath, value, sizeof(settings->logFilePath) -1);
  } else if (MATCH("server", "host")) {
    memcpy(settings->host, value, sizeof(settings->host) -1);
  } else if (MATCH("server", "port")) {
    settings->port = atoi(value);
  } else if (MATCH("server", "max_connections")) {
    settings->maxConnections = atoi(value);
  } else if (MATCH("server", "pid_file_path")) {
    memcpy(settings->pidFilePath, value, sizeof(settings->pidFilePath) -1);
  } else if (MATCH("tls", "cert_file")) {
    memcpy(settings->sslCertFilePath, value, sizeof(settings->sslCertFilePath) -1);
  } else if (MATCH("tls", "key_file")) {
    memcpy(settings->sslKeyFilePath, value, sizeof(settings->sslKeyFilePath));
  } else {
    return 0; // unknown section/name or error
  }
  return 1; // success
}

/**
 * Parses the command line options for the Start command
 * Returns true on success, false on failure
 * @param[in] argc The number of arguments
 * @param[in] argv The array of arguments
 * @param[in] settings Pointer to a configuration structure
 */
int parseOptions(int argc, ARGV argv, ServerConfigInfo *settings) {
  // Fill some defaults first
  GetDefaultLogFilePath(settings->logFilePath, sizeof(settings->logFilePath));
  GetDefaultPidFilePath(settings->pidFilePath, sizeof(settings->pidFilePath));
  GetDefaultTlsFilePaths(
    settings->sslCertFilePath, sizeof(settings->sslCertFilePath),
    settings->sslKeyFilePath, sizeof(settings->sslKeyFilePath)
  );
  char configFilePath[kMaxPath] = {};
  char sslCertFilePath[kMaxPath] = {};
  char sslKeyFilePath[kMaxPath] = {};
  char host[kMaxHostLength] = {};
  unsigned int port = 0;
  unsigned int maxConnections = 0;

  // Collect and parse command line options
  struct option options[] = {
    {"config-file", required_argument, NULL, 'c'},
    {"host", required_argument, NULL, 'h'},
    {"port", required_argument, NULL, 'p'},
    {"ssl-cert", required_argument, NULL, 's'}, // leaving c for config
    {"ssl-key", required_argument, NULL, 'k'},
    {"max-clients", required_argument, NULL, 'm'},
    {"foreground", no_argument, NULL, 'f'},
    { NULL, 0, NULL, 0}
  };

  int valid = true;

  char ch;
  while (true) {
    ch = getopt_long(argc, argv, "h:p:m:", options, NULL);
    if ((signed char)ch == -1) break; // no more options
    switch (ch) {
      case 'h':
        snprintf(host, sizeof(host) -1, "%s", optarg);
      break;
      case 'p':
        port = atoi(optarg);
      break;
      case 'm':
        maxConnections = atoi(optarg);
      break;
      case 'f':
        settings->foreground = true;
      break;
      case 's':
        snprintf(sslCertFilePath, sizeof(sslCertFilePath) -1, "%s", optarg);
      break;
      case 'k':
        snprintf(sslKeyFilePath, sizeof(sslKeyFilePath) -1, "%s", optarg);
      break;
      case 'c':
        snprintf(configFilePath, sizeof(configFilePath) -1, "%s", optarg);
      break;
      default:
        valid = false;
        break;
    }
  }
  if (strlen(configFilePath) == 0) {
    // No config file has been specified, use default
    GetConfigFilePath(configFilePath, sizeof(configFilePath));
  }
  if (strlen(configFilePath) > 0) {
    // We have a config file to parse
    int result = ini_parse(configFilePath, handler, settings);
    valid = (result == 0);

    memset(settings->configFilePath, 0, sizeof(settings->configFilePath));
    memcpy(settings->configFilePath, configFilePath, sizeof(settings->configFilePath) -1);

    if (result == -1) {
      // file open error
      fprintf(stderr, "unable to open file '%s' - %s\n", configFilePath, strerror(errno));
      return false;
    }
    if (result == -2) {
      // memory allocation error
      fprintf(stderr, "memory allocation error - %s\n", strerror(errno));
      return false;
    }
    if (result > 0) {
      // parse error at line result
      fprintf(stderr, "parse error in %s at line %d\n", configFilePath, result);
      return false;
    }
  }
  // Now we can override with CLI options, if specified
  if (strlen(host) > 0) {
    memset(settings->host, 0, sizeof(settings->host));
    memcpy(settings->host, host, sizeof(settings->host) -1);
  }
  if (port > 0) {
    settings->port = port;
  }
  if (maxConnections > 0) {
    settings->maxConnections = maxConnections;
  }
  if (strlen(sslCertFilePath) > 0) {
    memset(settings->sslCertFilePath, 0, sizeof(settings->sslCertFilePath));
    memcpy(settings->sslCertFilePath, sslCertFilePath, sizeof(settings->sslCertFilePath) -1);
  }
  if (strlen(sslKeyFilePath) > 0) {
    memset(settings->sslKeyFilePath, 0, sizeof(settings->sslKeyFilePath));
    memcpy(settings->sslKeyFilePath, sslKeyFilePath, sizeof(settings->sslKeyFilePath) -1);
  }
  return valid;
}
