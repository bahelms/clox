#pragma once
#include "chunk.h"

enum class InterpretResult {
  Ok,
  CompileError,
  RuntimeError,
};

class VM {
  static constexpr int STACK_MAX = 256;

  Chunk chunk{};
  const uint8_t *ip{};
  Value stack[STACK_MAX]{};
  Value *stack_top{};

  InterpretResult run();
  void push(Value value);
  Value pop();

public:
  VM();
  InterpretResult interpret(Chunk chunk);
};
