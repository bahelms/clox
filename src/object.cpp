#include <iostream>
#include <print>

#include "chunk.h"
#include "object.h"
#include "value.h"

ObjFunction::ObjFunction() : Object(ObjectType::Function), chunk(new Chunk()) {}

void print_object(const Value &value) {
  switch (value.as_object()->type) {
  case ObjectType::String:
    std::cout << value.as_string()->chars;
    break;
  case ObjectType::Function:
    ObjFunction *fn = value.as_function();
    if (!fn->name) {
      std::print("<script>");
      return;
    }
    std::print("<fn {}>", value.as_function()->name->chars);
    break;
  }
}
