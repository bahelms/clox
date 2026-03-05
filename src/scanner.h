#pragma once
#include <string_view>

enum class TokenType {
  // Single-character tokens
  LeftParen,
  RightParen,
  LeftBrace,
  RightBrace,
  Comma,
  Dot,
  Minus,
  Plus,
  Semicolon,
  Slash,
  Star,
  // One or two character tokens
  Bang,
  BangEqual,
  Equal,
  EqualEqual,
  Greater,
  GreaterEqual,
  Less,
  LessEqual,
  // Literals
  Identifier,
  String,
  Number,
  // Keywords
  And,
  Class,
  Else,
  False,
  For,
  Fun,
  If,
  Nil,
  Or,
  Print,
  Return,
  Super,
  This,
  True,
  Var,
  While,
  Error,
  Eof
};

struct Token {
  TokenType type;
  const char *start;
  int length;
  int line;
};

class Scanner {
  std::string_view src{};
  int line = 1;
  const char *start{};
  const char *current{};

public:
  Scanner(std::string_view source)
      : src(source), start(source.data()), current(source.data()) {}
  Token scan_token();
};
