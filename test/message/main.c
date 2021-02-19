/*
 * Copyright (C) 2020 Vito Tardia
 */

#include <stdio.h>
#include <string.h>

#include "message.h"
#include "message_tests.h"

int main() {

  TestMessage_getType();
  TestMessage_getContent();
  TestMessage_format();

  printf("\n");
  return 0;
}
