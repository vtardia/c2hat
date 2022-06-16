#ifndef _V_FS_UTIL_H_
#define _V_FS_UTIL_H_
  #include <stdbool.h>
  #include <sys/stat.h>
  #include <unistd.h>

  /**
   * Checks if the given directory exists and creates recursively
   * if it doesn't exist.
   *
   * Returns true if either the directory already exists or
   * it's created successfully.
   *
   * On error returns false and an error code is stored in errno.
   *
   * Returns false if one of the path component does not exist
   * or it's not a directory (ENOENT).
   */
  bool TouchDir(const char *path, mode_t mode);

  /**
   * Checks if the given path is a directory
   */
  bool IsDir(const char *path);

  /**
   * Checks if the given path is a file
   */
  bool IsFile(const char *path);

  /**
   * Checks if the given path is a symbolic link
   */
  bool IsLink(const char *path);

  /**
   * Checks if the given path is a FIFO
   */
  bool IsFiFo(const char *path);

  /**
   * Checks if the given path is a socket
   */
  bool IsSocket(const char *path);

  /**
   * Checks if a file or directory exists
   */
  #define Exists(path) (access(path, F_OK) == 0)

  /**
   * Checks if a file or directory is readable
   */
  #define IsReadable(path) (access(path, R_OK) == 0)

  /**
   * Checks if a file or directory is writeable
   */
  #define IsWritable(path) (access(path, W_OK) == 0)
#endif

