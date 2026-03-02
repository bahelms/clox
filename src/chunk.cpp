#include "chunk.h"
#include "doctest.h"

void Chunk::write(uint8_t byte, int line) {
  code.push_back(byte);
  int instruction_index = code.size() - 1;
  instruction_line_map[line].push_back(instruction_index);
}

// returns index of constant
int Chunk::add_constant(Value value) {
  constants.push_back(value);
  return constants.size() - 1;
}

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

TEST_CASE("Chunk::add_constant") {
  Chunk chunk;

  SUBCASE("returns incrementing indices for successive constants") {
    CHECK(chunk.add_constant(1.0) == 0);
    CHECK(chunk.add_constant(2.0) == 1);
    CHECK(chunk.add_constant(3.0) == 2);
  }
}
