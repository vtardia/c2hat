/*
 * Copyright (C) 2021 Vito Tardia
 */

#ifndef C2HAT_H
#define C2HAT_H

  #include <stddef.h>

  // This file contains enums and definitions that are common
  // between client and server

  enum {
    /// Max username length (in characters) excluding the NULL terminator
    kMaxNicknameLength = 15,
    /// Max username size in bytes, for Unicode characters
    kMaxNicknameSize = kMaxNicknameLength * sizeof(wchar_t),
    /// Max size of data that can be sent, including the NULL terminator
    kBufferSize = 1536,
    // Format is: /msg [<15charUsername>]:\s
    kBroadcastBufferSize = 9 + kMaxNicknameSize + kBufferSize,
    kMaxPath =  4096,
    kMaxHostLength =  40,
    kMaxLocaleLength =  25
  };

  #ifndef APPNAME
    #define APPNAME "c2hat"
  #endif
#endif

