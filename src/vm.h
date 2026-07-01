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
  ObjClosure *closure{};
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
  void reset_stack();
  bool call_value(Value callee, int arg_count);
  bool call(ObjClosure *closure, int arg_count);

  template <typename ValueBuilder, typename Op>
  bool binary_op(ValueBuilder builder, Op op);

  template <typename... Args>
  void runtime_error(std::format_string<Args...> fmt, Args &&...args);
  void define_native(const char *name, NativeFn function, int arity);

public:
  VM();
  ~VM();
  InterpretResult interpret(std::string source);
  ObjString *alloc_string(std::string s);
  ObjNative *alloc_native(NativeFn function, int arity);
  ObjClosure *alloc_closure(ObjFunction *fn);
  std::expected<uint8_t, const char *>
  get_or_alloc_global_slot(const std::string &name);
};

Value clock_native(int arg_count, Value *args);
