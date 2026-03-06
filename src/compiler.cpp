#include <iostream>
#include <string_view>

#include "scanner.h"

void compile(std::string source) {
  Scanner scanner{source};
  int line = -1;

  while (true) {
    Token token = scanner.scan_token();

    if (token.line != line) {
      std::cout << std::format("{:4d} ", token.line);
      line = token.line;
    } else {
      std::cout << "   | ";
    }
    std::cout << static_cast<int>(token.type) << " "
              << std::format("'{}'\n",
                             std::string_view(token.start, token.length));

    if (token.type == TokenType::Eof) {
      break;
    }
  }
}
