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

  bool is_at_end();
  Token make_token(TokenType type);
  char advance();
  bool match(char expected);
  void skip_whitespace();
  char peek_next();
  Token string();
  Token number();
  Token identifier();
  TokenType identifier_type();
  TokenType check_keyword(int start_idx, int length, const char *rest,
                          TokenType type);

public:
  Scanner(std::string_view source)
      : src(source), start(source.data()), current(source.data()) {}
  Token scan_token();
};
