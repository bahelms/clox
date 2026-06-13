#pragma once
#include <cstdint>
#include <expected>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "chunk.h"
#include "object.h"

static constexpr int UINT8_COUNT = UINT8_MAX + 1;
static constexpr int FRAMES_MAX = 64;
static constexpr int STACK_MAX = FRAMES_MAX * UINT8_COUNT;

enum class InterpretResult {
  Ok,
  CompileError,
  RuntimeError,
};

struct CallFrame {
  ObjFunction *function{};
  const uint8_t *ip{};
  Value *slots{};
};

class VM {
  Value stack[STACK_MAX]{};
  Value *stack_top{};
  Object *objects{};
  CallFrame frames[FRAMES_MAX];
  int frame_count{};

  std::unordered_map<std::string, ObjString *> interned_strings{};
  std::unordered_map<std::string, uint8_t> global_slots{};
  std::vector<Value> globals{};
  std::vector<bool> globals_defined{};
  std::vector<std::string> global_names{};

  InterpretResult run();
  void push(Value value);
  Value pop();
  Value peek(int distance);
  CallFrame &current_frame();
  void reset_stack();
  uint8_t read_byte();
  uint16_t read_short();
  Value read_constant();
  bool call_value(Value callee, int arg_count);
  bool call(ObjFunction *function, int arg_count);

  template <typename ValueBuilder, typename Op>
  InterpretResult binary_op(ValueBuilder builder, Op op);

  template <typename... Args>
  void runtime_error(std::format_string<Args...> fmt, Args &&...args);

public:
  VM();
  ~VM();
  InterpretResult interpret(std::string source);
  ObjString *alloc_string(std::string s);
  std::expected<uint8_t, const char *>
  get_or_alloc_global_slot(const std::string &name);
};
