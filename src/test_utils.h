#pragma once
#include <functional>
#include <iostream>
#include <sstream>
#include <string>

inline std::string capture_stdout(std::function<void()> fn) {
  std::ostringstream captured;
  std::streambuf *old = std::cout.rdbuf(captured.rdbuf());
  fn();
  std::cout.rdbuf(old);
  return captured.str();
}

inline void suppress_stderr(std::function<void()> fn) {
  std::ostringstream sink;
  std::streambuf *old = std::cerr.rdbuf(sink.rdbuf());
  fn();
  std::cerr.rdbuf(old);
}
