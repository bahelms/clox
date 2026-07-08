#include <iostream>
#include <print>

#include "chunk.h"
#include "doctest.h"
#include "object.h"
#include "test_utils.h"
#include "value.h"

// This is here since the header only has a forward declaration of Chunk.
// Actually using it is an implementation.
ObjFunction::ObjFunction()
    : Object(Object::Type::Function), chunk(new Chunk()) {}

void print_function(ObjFunction *fn) {
  if (!fn->name) {
    std::print("<script>");
    return;
  }
  std::print("<fn {}>", fn->name->chars);
}

void print_object(const Value &value) {
  switch (value.as_object()->type) {
  case Object::Type::String:
    std::print("{}", value.as_string()->chars);
    break;
  case Object::Type::Function: {
    print_function(value.as_function());
    break;
  }
  case Object::Type::Closure:
    print_function(value.as_closure()->function);
    break;
  case Object::Type::Upvalue:
    std::print("upvalue");
    break;
  case Object::Type::Native:
    std::print("<native fn>");
    break;
  }
}

TEST_CASE("print_object: function") {
  SUBCASE("named function prints <fn NAME>") {
    ObjFunction fn;
    ObjString name("foo");
    fn.name = &name;
    Value value = Value::object(&fn);
    CHECK(capture_stdout([&] { print_object(value); }) == "<fn foo>");
  }

  SUBCASE("nameless function prints <script>") {
    ObjFunction fn;
    Value value = Value::object(&fn);
    CHECK(capture_stdout([&] { print_object(value); }) == "<script>");
  }
}
