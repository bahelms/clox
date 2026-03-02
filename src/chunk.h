#pragma once
#include <unordered_map>
#include <vector>

#include "value.h"

enum OpCode { OP_CONSTANT, OP_RETURN };

class Chunk {
  std::vector<uint8_t> code{};
  std::unordered_map<int, std::vector<int>> instruction_line_map;

public:
  std::vector<Value> constants{};

  uint8_t operator[](int offset) const { return code[offset]; }
  auto size() const { return code.size(); }

  void write(uint8_t byte, int line);
  int add_constant(Value value);
  int get_line(int instruction_index) const;
};
