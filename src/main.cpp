#define DOCTEST_CONFIG_IMPLEMENT

#include "chunk.h"
#include "debug.h"

int main() {
  Chunk chunk{};

  chunk.write(OP_CONSTANT, 1);
  chunk.write_constant(7.32, 1);
  chunk.write(OP_RETURN, 1);

  disassemble_chunk(chunk, "test chunk");
  return 0;
}
