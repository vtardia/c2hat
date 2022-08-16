#ifndef SETTINGS_H
#define SETTINGS_H

  #include "client.h"
  #include "logger/logger.h"

  /// ARGV wrapper for options parsing
  typedef char * const * ARGV;

  /// Parses options from the command line into a ClientOptions structure
  void parseOptions(int argc, ARGV argv, ClientOptions *params);
#endif
