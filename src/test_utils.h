#pragma once
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>

inline std::string capture_stdout(std::function<void()> fn) {
  // Redirect fd 1 at the OS level so both std::print (FILE* stdout) and
  // std::cout (synced with stdio) are captured in the correct order.
  int pipefd[2];
  pipe(pipefd);
  std::cout.flush();
  fflush(stdout);
  int saved_fd = dup(STDOUT_FILENO);
  dup2(pipefd[1], STDOUT_FILENO);
  close(pipefd[1]);

  fn();

  std::cout.flush();
  fflush(stdout);
  dup2(saved_fd, STDOUT_FILENO);
  close(saved_fd);

  std::string result;
  char buf[4096];
  ssize_t n;
  while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
    result.append(buf, n);
  }
  close(pipefd[0]);
  return result;
}

inline std::string capture_stderr(std::function<void()> fn) {
  // Redirect fd 2 at the OS level so both std::print (FILE* stderr) and
  // std::cerr are captured in the correct order.
  int pipefd[2];
  pipe(pipefd);
  std::cerr.flush();
  fflush(stderr);
  int saved_fd = dup(STDERR_FILENO);
  dup2(pipefd[1], STDERR_FILENO);
  close(pipefd[1]);

  fn();

  std::cerr.flush();
  fflush(stderr);
  dup2(saved_fd, STDERR_FILENO);
  close(saved_fd);

  std::string result;
  char buf[4096];
  ssize_t n;
  while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
    result.append(buf, n);
  }
  close(pipefd[0]);
  return result;
}

inline void suppress_stderr(std::function<void()> fn) {
  std::ostringstream sink;
  std::streambuf *old = std::cerr.rdbuf(sink.rdbuf());
  fn();
  std::cerr.rdbuf(old);
}
