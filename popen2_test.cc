#include <vector>
#include <string>

#include "popen2.h"

#include "bricks/sync/waitable_atomic.h"

#include "3rdparty/gtest/gtest-main-with-dflags.h"  // IWYU pragma: keep

TEST(Popen2Test, Smoke) {
  std::string result;
  popen2({"/usr/bin/bash", "-c", "echo PASS"}, [&result](std::string const& line) { result = line; });
  EXPECT_EQ(result, "PASS");
}

TEST(Popen2Test, SmokeWithDelay) {
  std::string result;
  popen2({"/usr/bin/bash", "-c", "sleep 0.01; echo PASS2"}, [&result](std::string const& line) { result = line; });
  EXPECT_EQ(result, "PASS2");
}

TEST(Popen2Test, SmokeWithDelayInParentheses) {
  std::string result;
  popen2({"/usr/bin/bash", "-c", "(sleep 0.01; echo PASS3)"}, [&result](std::string const& line) { result = line; });
  EXPECT_EQ(result, "PASS3");
}

// TODO(dkorolev): This test does not need the `WaitableAtomic`. A different one does.
TEST(Popen2Test, ThreePrints) {
  std::string result;
  current::WaitableAtomic<int> c(0);
  popen2({"/usr/bin/bash", "-c", "echo ONE; sleep 0.01; echo TWO; sleep 0.01; echo THREE"},
         [&result, &c](std::string const& line) {
           result += line + ' ';
           ++*c.MutableScopedAccessor();
         });
  c.Wait([](int x) { return x == 3; });
  ASSERT_EQ(result, "ONE TWO THREE ");
}

TEST(Popen2Test, KillsSuccessfully) {
  bool nope = false;
  popen2(
      {"/usr/bin/bash", "-c", "sleep 10; echo NOPE"},
      [&nope](std::string const&) { nope = true; },
      [](Popen2Runtime& run) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        run.Kill();
      });
  ASSERT_TRUE(!nope);
}

TEST(Popen2Test, ReadsStdin) {
  std::string c;
  popen2(
      {"/usr/bin/bash", "-c", "read A; read B; echo $((A+B))"},
      [&c](std::string const& line) { c = line; },
      [](Popen2Runtime& run) { run("1\n2\n"); });
  ASSERT_EQ(c, "3");
}

TEST(Popen2Test, ReadsStdinForever) {
  std::string result = "result:";
  popen2(
      {"/usr/bin/bash",
       "-c",
       "while true; do read A; read B; C=$((A+B)); if [ $C == '0' ]; then exit; fi; echo $C; done"},
      [&result](std::string const& line) { result += ' ' + line; },
      [](Popen2Runtime& run) { run("1\n2\n3\n4\n0\n0\n"); });
  ASSERT_EQ(result, "result: 3 7");
}

TEST(Popen2Test, MultipleOutputLines) {
  std::string result = "result:";
  popen2({"/usr/bin/bash", "-c", "seq 10"}, [&result](std::string const& line) { result += ' ' + line; });
  ASSERT_EQ(result, "result: 1 2 3 4 5 6 7 8 9 10");
}

TEST(Popen2Test, MultipleOutputLinesWithMath) {
  std::string result = "result:";
  popen2({"/usr/bin/bash", "-c", "for i in $(seq 3 7) ; do echo $((i * i)) ; done"},
         [&result](std::string const& line) { result += ' ' + line; });
  ASSERT_EQ(result, "result: 9 16 25 36 49");
}

// TODO(dkorolev): This test does not need the `WaitableAtomic`.
TEST(Popen2Test, Env) {
  std::string result;
  current::WaitableAtomic<std::string> wa;
  popen2(
      {"/usr/bin/bash", "-c", "echo $FOO"},
      [&result](std::string const& s) { result = s; },
      [](Popen2Runtime&) {},
      {"FOO=bar"});
  ASSERT_TRUE(result == "bar");
}
