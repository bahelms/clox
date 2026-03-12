#include <iostream>
#include <string_view>

#include "parser.h"

void Parser::advance() {
  previous = current;
  while (true) {
    current = scanner.scan_token();
    if (current.type != TokenType::Error) {
      break;
    }

    error_at(current, current.start);
  }
}

// error is previous
void Parser::error_at(Token &token, const char *error_msg) {
  if (panic_mode) {
    return;
  }
  panic_mode = true;

  std::cerr << "[line " << token.line << "] Error";
  if (token.type == TokenType::Eof) {
    std::cerr << " at end";
  } else if (token.type == TokenType::Error) {
    // Nothing
  } else {
    std::cerr << " at '" << std::string_view(token.start, token.length) << "'";
  }

  std::cerr << ": " << error_msg << "\n";
  had_error = true;
}

void Parser::error(const char *message) { error_at(previous, message); }

void Parser::consume(TokenType type, const char *message) {
  if (current.type == type) {
    advance();
    return;
  }
  error_at(current, message);
}
