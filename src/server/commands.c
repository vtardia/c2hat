#include "commands.h"

#include <ctype.h>
#include <sys/stat.h>
#include <locale.h>
#include <libgen.h> // for basename() and dirname()

#include "encrypt/encrypt.h"

char *currentLogFilePath = NULL;
char *currentPIDFilePath = NULL;

/// Set to true when the server starts successfully
bool serverStartedSuccessfully = false;

/// Max length of a server command
const int kMaxCommandSize = 10;

/// Shared memory handle that stores the configuration data
char sharedMemPath[16] = {};

/// Size of the shared memory
const size_t kServerSharedMemSize = sizeof(ServerConfigInfo);

/// A 256 bit (32 bytes) key
const byte *kEncryptionKey = (byte *)EVP_ENCRYPTION_KEY;

/// A 128 bit (16 bytes) IV
const byte *kEncryptionIV = (byte *)EVP_ENCRYPTION_IV;

/// A large enough buffer to hold the encrypted config data
#define EncryptedServerConfigInfoSize (2 * sizeof(ServerConfigInfo))

/// Space in bytes needed to save the length of the encrypted content within the payload
#define EncryptedSizeOffset (sizeof(size_t))

/**
 * Sets the shared memory value depending on the current user
 */
void initSharedMemPath();

/**
 * Closes any open resource and deletes PID file and shared memory
 */
void clean();

/**
 * Clean facility called by STATUS and STOP commands
 * to clean the leftovers
 */
void cleanup();

/**
 * Parses the server command
 * Available commands are: start, stop, status
 * @param[in] argc The number of arguments for validation
 * @param[in] arg  The first argument from the ARGV array
 * @param[out]     The parsed command value
 */
Command parseCommand(int argc, const char *arg) {
  int result = kCommandUnknown;
  char *command = strndup(arg, kMaxCommandSize);
  for (char *c = command; *c != '\0'; c++) {
    *c = tolower(*c);
  }
  if (strcmp("start", command) == 0 && argc <= 9) {
    result = kCommandStart;
  }
  if (strcmp("stop", command) == 0 && argc == 2) {
    result = kCommandStop;
  }
  if (strcmp("status", command) == 0 && argc == 2) {
    result = kCommandStatus;
  }
  free(command);
  return result;
}

/**
 * Starts the server
 * @param[in] settings Pointer to a configuration structure
 */
int CMD_runStart(ServerConfigInfo *settings) {
  // Calling setlocale() with an empty string loads the LANG env var
  if (!setlocale(LC_ALL, "")) {
    fprintf(stderr, "Unable to read locale");
    return EXIT_FAILURE;
  }

  // Calling setlocale() with a NULL argument reads the corresponding LC_ var
  // If the system does not support the locale it will return "C"
  // otherwise the full locale string (e.g. en_US.UTF-8)
  char *locale = setlocale(LC_ALL, NULL);
  memcpy(settings->locale, locale, strlen(locale) + 1);

  // Check locale compatibility
  if (strstr(locale, "UTF-8") == NULL) {
    fprintf(stderr, "The given locale (%s) does not support UTF-8\n", locale);
    return EXIT_FAILURE;
  }

  if (strlen(settings->sslCertFilePath) == 0) {
    fprintf(stderr, "SSL certificate file path missing: use --ssl-cert=/path/to/cert.pem\n");
    return EXIT_FAILURE;
  }

  if (strlen(settings->sslKeyFilePath) == 0) {
    fprintf(stderr, "SSL private key file path missing: use --ssl-key=/path/to/key.pem\n");
    return EXIT_FAILURE;
  }

  initSharedMemPath();

  if (settings->foreground) {
    pid_t serverPID = getpid();
    fprintf(stdout, "Starting foreground server on %s:%d with locale '%s' and PID %d\n",
      settings->host, settings->port, settings->locale, serverPID
    );
  } else {
    pid_t sessionID;
    pid_t serverPID = fork();
    if (serverPID > 0) {
      // First fork, close the parent
      return EXIT_SUCCESS;
    }
    // serverPID = 0 means I'm in the child
    if (serverPID < 0) {
      fprintf(stderr, "Unable to start daemon server(1): %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    // I'm in the child, fork again
    serverPID = fork();
    if (serverPID < 0) {
      fprintf(stderr, "Unable to start daemon server(2): %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    if (serverPID > 0) {
      // I'm in second parent process
      fprintf(stdout, "Starting background server on %s:%d with locale '%s' and PID %d\n",
        settings->host, settings->port, settings->locale, serverPID
      );
      return EXIT_SUCCESS;
    }
    // I'm in the final child
    umask(0);
    chdir("/usr/local"); // TODO: this should depend on the user's privilege
    sessionID = setsid();
    if (sessionID < 0) {
      fprintf(stderr, "Unable to set new session: %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
  }

  // If foreground is enabled I'm the main process, otherwise I'm the child

  // Register shutdown function to close resource handlers
  atexit(clean);

  // Init log facility and close unused streams
  if (settings->foreground) {
    currentLogFilePath = NULL; // Log to stdout
  } else {
    // Try to create the log directory if not exists
    // TODO create a fileutil lib and call this TouchDir()
    currentLogFilePath = strdup(settings->logFilePath);
    char *path = strdup(settings->logFilePath);
    struct stat st = {};
    char *logDir = dirname(path);
    if (stat(logDir, &st) == -1) {
      if (mkdir(logDir, 0700) < 0) {
        if (path != NULL) free(path);
        Fatal("Unable to create log directory (%d)", logDir);
      }
    }
    if (path != NULL) free(path);
  }
  if (!vLogInit(LOG_INFO, currentLogFilePath)) {
    fprintf(stderr, "Unable to initialise the logger (%s): %s\n", currentLogFilePath, strerror(errno));
    fprintf(stdout, "Unable to initialise the logger (%s): %s\n", currentLogFilePath, strerror(errno));
    exit(EXIT_FAILURE);
  }
  close(STDIN_FILENO);
  close(STDOUT_FILENO);

  // Windows socket init
#if defined(_WIN32)
  WSADATA d;
  if (WSAStartup(MAKEWORD(2, 2), &d)) {
    fprintf(stderr, "Failed to initialize.\n");
    return 1;
  }
#endif

  // Initialise the chat server
  Server *server = Server_init(settings);

  // Init PID file (after server creation, so we don't create on failure)
  currentPIDFilePath = strdup(settings->pidFilePath);
  if (currentPIDFilePath == NULL) {
    Error("Unable to initialise PID file '%s'", currentPIDFilePath);
    return EXIT_FAILURE;
  }
  pid_t pid = PID_init(currentPIDFilePath);

  // The PID file can be safely deleted on exit
  serverStartedSuccessfully = true;

  // Update the configuration and write it to a shared memory location
  settings->pid = pid;
  if (currentLogFilePath != NULL) free(currentLogFilePath);

  // Encrypt settings first...
  // Since encryption and decryption happen in separate processes,
  // we need to communicate the size of the encrypted data to the
  // decrypting process, or a SegFault is generated.
  // In order to do this we reserve the first bytes of the encrypted
  // payload to store the length so the encrypted data start at
  // encryptedSettings + sizeof(size_t)
  byte encryptedSettings[EncryptedServerConfigInfoSize] = {};
  size_t encryptedSettingsSize = AES_encrypt(
    (byte *)settings,
    sizeof(ServerConfigInfo),
    kEncryptionKey,
    kEncryptionIV,
    encryptedSettings + EncryptedSizeOffset // Start writing after the offset
  );
  if ((int)encryptedSettingsSize < 0) {
    Error("Unable to encrypt settings");
    return EXIT_FAILURE;
  }

  // ...at this point we copy the length of the encrypted data
  // at the beginning of the payload we are about to save
  // and save the encrypted version
  memcpy(encryptedSettings, &encryptedSettingsSize, EncryptedSizeOffset);
  if (!Config_save(
        encryptedSettings,
        encryptedSettingsSize + EncryptedSizeOffset,
        sharedMemPath
     )
  ) {
    return EXIT_FAILURE;
  }

  Info(
    "Starting on %s:%d with PID %u and %d clients...",
    settings->host, settings->port,
    settings->pid, settings->maxConnections
  );

  // Start the chat server (infinite loop until SIGTERM)
  Server_start(server);

  // Windows socket cleanup
#if defined(_WIN32)
  WSACleanup();
#endif

  return EXIT_SUCCESS;
}

/**
 * Stop the running server
 */
int CMD_runStop() {
  // Load configuration
  initSharedMemPath();
  // First we read the size of the encrypted payload
  size_t *encryptedSettingsSize = (size_t *)Config_load(
    sharedMemPath, EncryptedSizeOffset
  );
  if (encryptedSettingsSize == NULL) {
    if (errno == ENOENT) {
      fprintf(stderr, "The server may not be running\n");
    } else {
      fprintf(stderr,
        "Unable to load configuration size from shared memory: %s\n", strerror(errno)
      );
    }
    return EXIT_FAILURE;
  }

  // Then we try to read the settings starting at the offset
  // pointed by the size
  byte *encryptedSettings = (byte *)Config_load(
    sharedMemPath, *encryptedSettingsSize + EncryptedSizeOffset
  );
  // If we could read above, the server is running
  if (encryptedSettings == NULL) {
    fprintf(stderr, "Unable to load configuration from shared memory: %s\n", strerror(errno));
    free(encryptedSettingsSize);
    return EXIT_FAILURE;
  }

  // Now we can decrypt...
  ServerConfigInfo settings = {};
  size_t decryptedSettingsSize = AES_decrypt(
    encryptedSettings + EncryptedSizeOffset,
    *encryptedSettingsSize,
    kEncryptionKey,
    kEncryptionIV,
    (byte *)&settings
  );
  // ...and cleanup
  free(encryptedSettingsSize);
  if ((int)decryptedSettingsSize < 0) {
    Error("Unable to decrypt settings");
    return EXIT_FAILURE;
  }

  currentPIDFilePath = strdup(settings.pidFilePath);
  int pidStatus = PID_exists(settings.pid);
  int result;

  // Existing PID
  if (pidStatus > 0) {
    printf("The server is running with PID %d\n", settings.pid);
    if (kill(settings.pid, SIGTERM) < 0) {
      printf("Unable to kill process %d: %s\n", settings.pid, strerror(errno));
      result = EXIT_FAILURE;
    } else {
      printf("The server with PID %d has been successfully stopped\n", settings.pid);
      // The server daemon will take care of cleaning the PID file and shared memory
      result = EXIT_SUCCESS;
    }
  }

  // Non-existing PID
  if (pidStatus == 0) {
    printf("Unable to check for PID %d: the server may not be running\n", settings.pid);
    // Enable cleaning the shared memory and PID file
    cleanup();
    result = EXIT_FAILURE;
  }

  // Error checking PID
  // We don't delete the PID file or shared memory because
  // the current user may not have access permissions the PID file
  if (pidStatus < 0 ) {
    printf("Error while checking for PID %d: %s\n", settings.pid, strerror(errno));
    result = EXIT_FAILURE;
  }

  // Cleanup configuration data
  if (currentPIDFilePath != NULL) free(currentPIDFilePath);
  memset(encryptedSettings, 0, EncryptedServerConfigInfoSize);
  free(encryptedSettings);
  encryptedSettings = NULL;

  return result;
}

/**
 * Check the status of the server daemon
 */
int CMD_runStatus() {
  // Load configuration
  initSharedMemPath();
  size_t *encryptedSettingsSize = (size_t *)Config_load(
    sharedMemPath, EncryptedSizeOffset
  );
  if (encryptedSettingsSize == NULL) {
    if (errno == ENOENT) {
      fprintf(stderr, "The server may not be running\n");
    } else {
      fprintf(stderr,
        "Unable to load configuration size from shared memory: %s\n", strerror(errno)
      );
    }
    return EXIT_FAILURE;
  }

  byte *encryptedSettings = (byte *)Config_load(
    sharedMemPath, *encryptedSettingsSize + EncryptedSizeOffset
  );
  if (encryptedSettings == NULL) {
    fprintf(stderr,
      "Unable to load configuration from shared memory: %s\n", strerror(errno)
    );
    free(encryptedSettingsSize);
    return EXIT_FAILURE;
  }

  ServerConfigInfo settings = {};
  size_t decryptedSettingsSize = AES_decrypt(
    encryptedSettings + EncryptedSizeOffset,
    *encryptedSettingsSize,
    kEncryptionKey,
    kEncryptionIV,
    (byte *)&settings
  );
  free(encryptedSettingsSize);
  if ((int)decryptedSettingsSize < 0) {
    Error("Unable to decrypt settings");
    return EXIT_FAILURE;
  }

  int pidStatus = PID_exists(settings.pid);
  currentPIDFilePath = strdup(settings.pidFilePath);
  int result;
  switch (pidStatus) {
    // The process exists
    case 1:
      printf("\nThe server is running with the following configuration:\n");
      printf("         PID: %d\n", settings.pid);
      printf(
        " Config file: %s\n",
        strlen(settings.configFilePath) > 0 ? settings.configFilePath : "(none)"
      );
      printf("    Log file: %s\n", settings.logFilePath);
      printf("    PID file: %s\n", settings.pidFilePath);
      printf("        Host: %s\n", settings.host);
      printf("        Port: %d\n", settings.port);
      printf("    SSL cert: %s\n", settings.sslCertFilePath);
      printf("     SSL key: %s\n", settings.sslKeyFilePath);
      printf("      Locale: %s\n", settings.locale);
      printf(" Max Clients: %d\n", settings.maxConnections);
      printf("\n");
      result = EXIT_SUCCESS;
    break;

    // The process does not exist
    case 0:
      printf("Unable to check for PID %d: the server may not be running\n", settings.pid);
      // Enable cleaning the shared memory and PID file
      cleanup();
      result = EXIT_FAILURE;
    break;

    // Cannot access the process info
    default:
      printf("Error while checking for PID %d: %s\n", settings.pid, strerror(errno));
      result = EXIT_FAILURE;
    break;
  }

  // Cleanup configuration data
  if (currentPIDFilePath != NULL) free(currentPIDFilePath);
  // TODO: wrap into a macro and use memset_s/explicit_bzero
  memset(encryptedSettings, 0, EncryptedServerConfigInfoSize);
  free(encryptedSettings);
  encryptedSettings = NULL;

  return result;
}

/**
 * Closes any open resource and deletes PID file and shared memory
 */
void clean() {
  Info("Cleaning up...");
  // Remove PID file only if it was created by a successful server start
  if (serverStartedSuccessfully) {
    if (currentPIDFilePath != NULL) {
      if (remove(currentPIDFilePath) < 0) {
        Error("Unable to remove PID file: %s", strerror(errno));
      }
      free(currentPIDFilePath);
    }
    if (!Config_clean(sharedMemPath)) {
      Error("Unable to clean configuration: %s", strerror(errno));
    }
  }
}

/**
 * Clean facility called by STATUS and STOP commands
 * to clean the leftovers
 */
void cleanup() {
  if (currentPIDFilePath != NULL) {
    if (remove(currentPIDFilePath) < 0) {
      fprintf(stderr, "Unable to remove PID file: %s", strerror(errno));
    }
    free(currentPIDFilePath);
    currentPIDFilePath = NULL;
  }
  if (!Config_clean(sharedMemPath)) {
    fprintf(stderr, "Unable to clean configuration: %s", strerror(errno));
  }
}

/**
 * Sets the shared memory value depending on the current user
 */
void initSharedMemPath() {
  uid_t uid = getuid();
  if (uid == 0) { // running as root, use /<app>
    snprintf(sharedMemPath, sizeof(sharedMemPath), "/%s", APPNAME);
  } else { // running as normal user, use /<app>-uid
    snprintf(sharedMemPath, sizeof(sharedMemPath), "/%s-%d", APPNAME, uid);
  }
}

