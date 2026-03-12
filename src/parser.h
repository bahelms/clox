#pragma once
#include "scanner.h"

class Parser {
  Scanner scanner;
  Token current{};
  bool panic_mode{};

public:
  Token previous{};
  bool had_error{};
  TokenType current_type() { return current.type; };
  TokenType previous_type() { return previous.type; };

  Parser(std::string_view source) : scanner(source) {}
  void advance();
  void error_at(Token &token, const char *error_msg);
  void error(const char *error_msg);
  void consume(TokenType type, const char *message);
};
