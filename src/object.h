#pragma once
#include <string>

struct Value;
class Chunk;

enum class ObjectType { String, Function };

class Object {
public:
  ObjectType type{};
  Object *next{};

  explicit Object(ObjectType t) : type(t) {}
};

class ObjString : public Object {
public:
  std::string chars;

  explicit ObjString(std::string s)
      : Object(ObjectType::String), chars(std::move(s)) {}
};

class ObjFunction : public Object {
public:
  int arity{};
  ObjString *name{};
  Chunk *chunk{};

  ObjFunction();
};

void print_object(const Value &value);
