cmake_minimum_required(VERSION 3.14...3.26)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  dplang
  VERSION 1.0
  DESCRIPTION "DataPanel Language"
  LANGUAGES C
)

find_program(CLANG_TIDY_EXE NAMES "clang-tidy" REQUIRED)
set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "-checks=-*,bugprone-*,portability-*,readability-*,-readability-identifier-length,performance-*,misc-*,clang-analyzer-*,-clang-analyzer-security*,-readability-else-after-return,-bugprone-branch-clone")


add_executable(dplang chunk.c compiler.c main.c memory.c scanner.c value.c vm.c object.c table.c hash.c parser.c builtins.c)
target_link_libraries(dplang m)

set_target_properties(dplang PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
