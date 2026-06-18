#pragma once
#include <string>

struct Value;
class Chunk;

class Object {
public:
  enum class Type { String, Function, Native };

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

  ObjFunction();
};

using NativeFn = Value (*)(int arg_count, Value *args);

struct ObjNative : public Object {
  NativeFn function{};
  int arity{};

  ObjNative(NativeFn fn, int arity)
      : Object(Type::Native), function(fn), arity(arity) {}
};

void print_object(const Value &value);
