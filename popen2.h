#pragma once

#include <atomic>
#include <iostream>

#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/eventfd.h>
#include <poll.h>

#include <string>
#include <thread>
#include <functional>

#include "bricks/strings/group_by_lines.h"

template <typename F>
static void MutableCStyleVectorStringsArg(const std::vector<std::string>& in, F&& f) {
  std::vector<std::vector<char>> mutable_in;
  mutable_in.resize(in.size());
  for (size_t i = 0u; i < in.size(); ++i) {
    mutable_in[i].assign(in[i].c_str(), in[i].c_str() + in[i].length() + 1u);
  }
  std::vector<char*> out;
  for (auto& e : mutable_in) {
    out.push_back(&e[0]);
  }
  out.push_back(nullptr);
  f(&out[0]);
}

inline void pipe_or_fail(int r[2]) {
  if (pipe(r)) {
    std::cerr << "FATAL: " << __LINE__ << std::endl;
    ::abort();
  }
}

inline int popen2(std::vector<std::string> const& cmdline_,
                  std::function<void(const std::string&)> cb_line,
                  std::function<void(std::function<void(std::string const&)>, std::function<void()>)> cb_code,
                  std::vector<std::string> const& env_ = {}) {
  pid_t pid;
  int pipe_stdin[2];
  int pipe_stdout[2];

  pipe_or_fail(pipe_stdin);
  pipe_or_fail(pipe_stdout);

  int const efd = eventfd(0, 0);
  if (efd < 0) {
    std::cerr << "FATAL: " << __LINE__ << std::endl;
    ::abort();
  }

  pid = fork();
  if (pid < 0) {
    std::cerr << "FATAL: " << __LINE__ << std::endl;
    ::abort();
  }

  if (pid == 0) {
    // Child.
    ::close(pipe_stdin[1]);
    dup2(pipe_stdin[0], 0);
    ::close(pipe_stdout[0]);
    dup2(pipe_stdout[1], 1);

    if (env_.empty()) {
      MutableCStyleVectorStringsArg(cmdline_, [&](char* const argv[]) {
        int r = execvp(cmdline_[0].c_str(), argv);
        std::cerr << "FATAL: " << __LINE__ << " R=" << r << ", errno=" << errno << std::endl;
        perror("execvp");
      });
    } else {
      MutableCStyleVectorStringsArg(cmdline_, [&](char* const argv[]) {
        MutableCStyleVectorStringsArg(env_, [&](char* const envp[]) {
          int r = execvpe(cmdline_[0].c_str(), argv, envp);
          std::cerr << "FATAL: " << __LINE__ << " R=" << r << ", errno=" << errno << std::endl;
          perror("execvpe");
        });
      });
    }
  }

  ::close(pipe_stdin[0]);
  ::close(pipe_stdout[1]);

  std::shared_ptr<std::atomic_bool> already_done = std::make_shared<std::atomic_bool>(false);
  std::thread thread_user_code(
      [copy_already_done = already_done](
          std::function<void(std::function<void(std::string const&)>, std::function<void()>)> cb_code,
          int write_fd,
          int pid) {
        cb_code(
            [write_fd](std::string const& s) {
              ssize_t const n = write(write_fd, s.c_str(), s.length());
              if (n < 0 || static_cast<size_t>(n) != s.length()) {
                return false;
              } else {
                return true;
              }
            },
            [pid, moved_already_done = std::move(copy_already_done)]() {
              if (!*moved_already_done) {
                *moved_already_done = true;
                kill(pid, SIGTERM);
              }
            });
      },
      std::move(cb_code),
      pipe_stdin[1],
      pid);

  std::thread thread_reader(
      [](std::function<void(const std::string&)> cb_line, int read_fd, int efd) {
        struct pollfd fds[2];
        fds[0].fd = read_fd;
        fds[0].events = POLLIN;
        fds[1].fd = efd;
        fds[1].events = POLLIN;

        char buf[1000];

        auto grouper = current::strings::CreateStatefulGroupByLines(std::move(cb_line));

        while (true) {
          ::poll(fds, 2, -1);
          if (fds[0].revents & POLLIN) {
            ssize_t const n = read(read_fd, buf, sizeof(buf) - 1);
            if (n < 0) {
              // NOTE(dkorolev): This may or may not be a major issue.
              // std::cerr << __LINE__ << std::endl;
              break;
            }
            buf[n] = '\0';
            grouper.Feed(buf);
          } else if (fds[1].revents & POLLIN) {
            // NOTE(dkorolev): Termination signaled.
            // std::cerr << __LINE__ << std::endl;
            break;
          }
        }
      },
      std::move(cb_line),
      pipe_stdout[0],
      efd);

  ::waitpid(pid, NULL, 0);
  *already_done = true;

  uint64_t u = 1;
  if (write(efd, &u, sizeof(uint64_t)) != sizeof(uint64_t)) {
    std::cerr << "FATAL: " << __LINE__ << std::endl;
    ::abort();
  }

  thread_user_code.join();
  thread_reader.join();

  ::close(efd);

  ::close(pipe_stdin[1]);
  ::close(pipe_stdout[0]);

  return 0;
}
