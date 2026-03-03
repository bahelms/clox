#pragma once
#include <unordered_map>
#include <vector>

#include "value.h"

enum OpCode { OP_CONSTANT, OP_RETURN };

class Chunk {
  std::vector<uint8_t> code{};
  std::unordered_map<int, std::vector<int>> instruction_line_map;
  std::vector<Value> constants{};

public:
  uint8_t operator[](int offset) const { return code[offset]; }
  auto size() const { return code.size(); }
  const uint8_t *data() const { return code.data(); }

  void write(uint8_t byte, int line);
  void write_constant(Value value, int line);
  Value get_constant(int index) const;
  int get_line(int instruction_index) const;
};
