#define DOCTEST_CONFIG_IMPLEMENT

#include <fstream>
#include <iostream>

#include "vm.h"

namespace {
void repl() {
  VM vm{};
  std::string line;
  while (true) {
    std::cout << "> ";
    if (!std::getline(std::cin, line)) {
      std::cout << '\n';
      break;
    }
    vm.interpret(line);
  }
}

std::string read_file(const char *path) {
  std::ifstream file(path);
  if (!file) {
    std::cerr << "Could not open file: " << path << '\n';
    exit(74);
  }
  return std::string(std::istreambuf_iterator<char>(file),
                     std::istreambuf_iterator<char>());
}

void run_file(const char *path) {
  VM vm{};
  InterpretResult result = vm.interpret(read_file(path));
  if (result == InterpretResult::CompileError) {
    exit(65);
  }
  if (result == InterpretResult::RuntimeError) {
    exit(70);
  }
}

} // namespace

int main(int argc, const char *argv[]) {
  if (argc == 1) {
    repl();
  } else if (argc == 2) {
    run_file(argv[1]);
  } else {
    std::cerr << "Usage: clox [path]\n";
    exit(64);
  }

  // Chunk chunk{};
  // chunk.write(OP_CONSTANT, 1);
  // chunk.write_constant(1.2, 1);
  // chunk.write(OP_CONSTANT, 1);
  // chunk.write_constant(3.4, 1);
  // chunk.write(OP_ADD, 1);
  // chunk.write(OP_CONSTANT, 1);
  // chunk.write_constant(5.6, 1);
  // chunk.write(OP_DIVIDE, 1);
  // chunk.write(OP_NEGATE, 1);
  // chunk.write(OP_RETURN, 1);

  // vm.interpret(std::move(chunk));
  return 0;
}
