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
    /// Max length of a filesystem path
    kMaxPath =  4096,
    /// Max length for a host parameter
    kMaxHostLength =  40,
    /// Max length of the system locale string (e.g. en_GB.UTF-8)
    kMaxLocaleLength =  25
  };

  // The application name can be customised at compile time
  #ifndef APPNAME
    #define APPNAME "c2hat"
  #endif
#endif

