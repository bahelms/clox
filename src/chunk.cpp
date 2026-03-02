#include "chunk.h"
#include "doctest.h"

void Chunk::write(uint8_t byte, int line) {
  code.push_back(byte);
  int instruction_index = code.size() - 1;
  instruction_line_map[line].push_back(instruction_index);
}

void Chunk::write_constant(Value value, int line) {
  constants.push_back(value);
  int index = constants.size() - 1;
  write(index, line);
}

Value Chunk::get_constant(int index) const { return constants[index]; }

int Chunk::get_line(int instruction_index) const {
  for (const auto &pair : instruction_line_map) {
    auto instr_indexes = pair.second;
    auto it = std::find(instr_indexes.begin(), instr_indexes.end(),
                        instruction_index);
    if (it != instr_indexes.end()) {
      return pair.first;
    }
  }
  return 0;
}

TEST_CASE("Chunk::write_constant") {
  Chunk chunk;

  SUBCASE("sets the constant with the line") {
    chunk.write_constant(1.01, 6);
    CHECK(chunk.get_constant(0) == 1.01);
  }
}
