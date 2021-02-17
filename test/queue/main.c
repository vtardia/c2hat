/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdio.h>

#include "queue/queue.h"
#include "queue_tests.h"

int main() {
  TestQueue_new();
  TestQueue_enqueue();
  TestQueue_dequeue();
  TestQueue_peek();
  TestQueue_lpeek();
  TestQueue_purge();
  printf("\n");
  return 0;
}
