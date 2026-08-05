#pragma once
#include <string>
#include <vector>

#include "value.h"

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
  ObjUpvalue *next{};
  Value closed{};

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

// Value's object-dependent methods, defined here now that the Obj* types are
// complete. Kept inline to preserve inlining in hot paths.
inline Value Value::object(ObjString *obj_str) {
  return {ValueType::Object, {.object = obj_str}};
}
inline Value Value::object(ObjFunction *obj_func) {
  return {ValueType::Object, {.object = obj_func}};
}
inline Value Value::object(ObjClosure *obj_closure) {
  return {ValueType::Object, {.object = obj_closure}};
}
inline Value Value::object(ObjNative *obj_native) {
  return {ValueType::Object, {.object = obj_native}};
}

inline bool Value::is_string() {
  return is_object() && as_object()->type == Object::Type::String;
}
inline ObjString *Value::as_string() const {
  return static_cast<ObjString *>(as_object());
}

inline bool Value::is_function() {
  return is_object() && as_object()->type == Object::Type::Function;
}
inline ObjFunction *Value::as_function() const {
  return static_cast<ObjFunction *>(as_object());
}

inline bool Value::is_closure() {
  return is_object() && as_object()->type == Object::Type::Closure;
}
inline ObjClosure *Value::as_closure() const {
  return static_cast<ObjClosure *>(as_object());
}

inline bool Value::is_native() {
  return is_object() && as_object()->type == Object::Type::Native;
}
inline ObjNative *Value::as_native() const {
  return static_cast<ObjNative *>(as_object());
}
