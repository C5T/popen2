cmake_minimum_required(VERSION 3.14.1)

# The project should be called `C5T_{UPPERCASE_REPO_NAME}`.
project(C5T_POPEN2 LANGUAGES C CXX)

# To see this repo is included.
message(STATUS "The `popen2` C5T module was added on the `cmake` level.")

# A standard C++ `cmake` project.
add_library(C5T_POPEN2 SHARED popen2.cc)
target_include_directories(C5T_POPEN2 INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

# And a test.
# NOTE(dkorolev): Do not mark it as such for `cmake` / `ctest`! They do not play well with `fork()`.
#enable_testing() <-- commented out by design.
add_executable(popen2_test popen2_test.cc)
get_property(C5T_PATH_VALUE GLOBAL PROPERTY C5T_PATH)
message(STATUS "DIMA VALUE: ${C5T_PATH_VALUE}")
target_include_directories(popen2_test PRIVATE ${C5T_PATH_VALUE})
#add_test(NAME popen2_test COMMAND popen2_test) <-- commented out by design.
