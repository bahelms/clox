#pragma once
#include "chunk.h"

enum InterpretResult {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
};

class VM {
  Chunk *chunk{};
  const uint8_t *ip{};

  InterpretResult run();

public:
  InterpretResult interpret(Chunk *chunk);
};
