#pragma once
#include <string>
#include <vector>

struct Value;
class Chunk;

class Object {
public:
  enum class Type { String, Function, Closure, Native, Upvalue };

  Type type{};
  Object *next{};

  explicit Object(Type t) : type(t) {}
};

class ObjString : public Object {
public:
  std::string chars;

  explicit ObjString(std::string s)
      : Object(Type::String), chars(std::move(s)) {}
};

class ObjFunction : public Object {
public:
  int arity{};
  ObjString *name{};
  Chunk *chunk{};
  int upvalue_count{};

  ObjFunction();
};

struct ObjUpvalue : public Object {
  Value *location{};

  ObjUpvalue(Value *slot) : Object(Type::Upvalue), location(slot) {}
};

struct ObjClosure : public Object {
  ObjFunction *function{};
  int upvalue_count{}; // needed for GC
  std::vector<ObjUpvalue *> upvalues{};

  ObjClosure(ObjFunction *fn)
      : Object(Type::Closure), function(fn), upvalue_count(fn->upvalue_count),
        upvalues(fn->upvalue_count, nullptr) {}
};

using NativeFn = Value (*)(int arg_count, Value *args);

struct ObjNative : public Object {
  NativeFn function{};
  int arity{};

  ObjNative(NativeFn fn, int arity)
      : Object(Type::Native), function(fn), arity(arity) {}
};

void print_object(const Value &value);
