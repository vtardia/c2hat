/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdio.h>
#include <string.h>

#include "list/list.h"
#include "list_tests.h"

int main() {

  TestList_new();

  TestList_next();
  TestList_prev();
  TestList_first();
  TestList_last();
  TestList_item();
  TestList_rewind();

  TestList_append();
  TestList_prepend();
  TestList_insert();
  TestList_update();
  TestList_delete();

  TestList_search();

  TestList_sort();

  printf("\n");
  return 0;
}
