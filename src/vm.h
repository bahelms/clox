#pragma once
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
  std::unordered_map<std::string, uint8_t> global_slots{};
  std::vector<Value> globals{};
  std::vector<bool> globals_defined{};
  std::vector<std::string> global_names{};

  InterpretResult run();
  void push(Value value);
  Value pop();
  Value peek(int distance);
  void reset_stack();
  uint8_t read_byte();
  uint16_t read_short();

  template <typename ValueBuilder, typename Op>
  InterpretResult binary_op(ValueBuilder builder, Op op);

  template <typename... Args>
  void runtime_error(std::format_string<Args...> fmt, Args &&...args);

public:
  VM();
  ~VM();
  InterpretResult interpret(std::string source);
  ObjString *alloc_string(std::string s);
  std::optional<uint8_t> get_or_alloc_global_slot(const std::string &name);
};
