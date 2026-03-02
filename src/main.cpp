#define DOCTEST_CONFIG_IMPLEMENT

#include "chunk.h"
#include "debug.h"

int main() {
  Chunk chunk{};

  chunk.write(OP_CONSTANT, 1);
  const int constant_index = chunk.add_constant(1.2);
  chunk.write(constant_index, 1);

  chunk.write(OP_RETURN, 1);
  disassemble_chunk(chunk, "test chunk");
  return 0;
}
