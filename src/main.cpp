#define DOCTEST_CONFIG_IMPLEMENT

#include "chunk.h"
#include "vm.h"

int main() {
  VM vm{};
  Chunk chunk{};

  chunk.write(OP_CONSTANT, 1);
  chunk.write_constant(7.32, 1);
  chunk.write(OP_RETURN, 1);

  vm.interpret(&chunk);
  return 0;
}
