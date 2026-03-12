#include "chunk.h"
#include "doctest.h"

void Chunk::write(uint8_t byte, int line) {
  code.push_back(byte);
  int instruction_index = code.size() - 1;
  instruction_line_map[line].push_back(instruction_index);
}

uint8_t Chunk::write_constant(Value value) {
  constants.push_back(value);
  return constants.size() - 1;
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

TEST_CASE("Chunk::write") {
  Chunk chunk;

  SUBCASE("writes byte into code and increases size") {
    chunk.write(OP_RETURN, 1);
    CHECK(chunk.size() == 1);
    CHECK(chunk[0] == OP_RETURN);
  }

  SUBCASE("multiple writes accumulate in order") {
    chunk.write(OP_CONSTANT, 1);
    chunk.write(OP_RETURN, 1);
    CHECK(chunk.size() == 2);
    CHECK(chunk[0] == OP_CONSTANT);
    CHECK(chunk[1] == OP_RETURN);
  }

  SUBCASE("data() points to the first byte") {
    chunk.write(OP_RETURN, 1);
    CHECK(*chunk.data() == OP_RETURN);
  }
}

TEST_CASE("Chunk::get_line") {
  Chunk chunk;

  SUBCASE("returns the line a byte was written on") {
    chunk.write(OP_RETURN, 42);
    CHECK(chunk.get_line(0) == 42);
  }

  SUBCASE("returns 0 for an index with no matching line") {
    CHECK(chunk.get_line(0) == 0);
  }

  SUBCASE("distinguishes instructions on different lines") {
    chunk.write(OP_CONSTANT, 10);
    chunk.write(OP_RETURN, 20);
    CHECK(chunk.get_line(0) == 10);
    CHECK(chunk.get_line(1) == 20);
  }
}

TEST_CASE("Chunk::write_constant") {
  Chunk chunk;

  SUBCASE("stores the value retrievable via get_constant") {
    chunk.write_constant(1.01);
    CHECK(chunk.get_constant(0) == 1.01);
  }

  SUBCASE("does not write anything to code") {
    chunk.write_constant(1.01);
    CHECK(chunk.size() == 0);
  }

  SUBCASE("successive constants return increasing indices") {
    CHECK(chunk.write_constant(1.0) == 0);
    CHECK(chunk.write_constant(2.0) == 1);
  }
}
