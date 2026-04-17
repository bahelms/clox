#pragma once
#include <cstdint>
#include <iostream>
#include <unordered_map>

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
  Object *objects{};
  std::unordered_map<std::string, ObjString *> interned_strings{};
  std::unordered_map<std::string, Value> globals{};

  InterpretResult run();
  void push(Value value);
  Value pop();
  Value peek(int distance);
  void reset_stack();
  uint8_t read_byte();
  ObjString *read_string();

  template <typename ValueBuilder, typename Op>
  InterpretResult binary_op(ValueBuilder builder, Op op);

  template <typename... Args>
  void runtime_error(std::format_string<Args...> fmt, Args &&...args);

public:
  VM();
  ~VM();
  InterpretResult interpret(std::string source);
  ObjString *alloc_string(std::string s);
};
