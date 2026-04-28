#pragma once
#include <unordered_map>
#include <vector>

#include "value.h"

enum OpCode {
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_POPN,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_SET_GLOBAL,
  OP_EQUAL,
  OP_NOT_EQUAL,
  OP_GREATER,
  OP_GREATER_EQUAL,
  OP_LESS,
  OP_LESS_EQUAL,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_RETURN
};

class Chunk {
  std::vector<uint8_t> code{};
  std::unordered_map<int, std::vector<int>> instruction_line_map;
  std::vector<Value> constants{};

public:
  uint8_t operator[](int offset) const { return code[offset]; }
  auto size() const { return code.size(); }
  const uint8_t *data() const { return code.data(); }

  void write(uint8_t byte, int line);
  uint8_t write_constant(Value value);
  Value get_constant(int index) const;
  int get_line(int instruction_index) const;
};
