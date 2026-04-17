#pragma once
#include "scanner.h"

class Parser {
  Scanner scanner;
  Token current{};
  bool panic_mode{};

public:
  Token previous{};
  bool had_error{};
  TokenType current_type() { return current.type; }
  TokenType previous_type() { return previous.type; }
  bool has_panicked() { return panic_mode; }
  void panic(bool value) { panic_mode = value; }

  Parser(std::string_view source) : scanner(source) {}
  void advance();
  void error_at(Token &token, const char *error_msg);
  void error(const char *error_msg);
  void consume(TokenType type, const char *message);
};
