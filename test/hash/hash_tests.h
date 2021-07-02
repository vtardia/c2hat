/**
 * Copyright (C) 2021 Vito Tardia
 *
 * This file is part of vHashLib.
 *
 * vHashLib is a simple C implementation of hashes
 * (associative arrays) using Hash Tables.
 *
 * vHashLib is free software; you can redistribute it and/or
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

#ifndef HASH_TEST_H
#define HASH_TEST_H

  // Tests new, free, empty
  void TestHash_new();

  // Tests get and set
  void TestHash_set();

  void TestHash_delete();

  void TestHash_first();
  void TestHash_last();

  void TestHash_unicode();
  void TestHash_bulk();
#endif

