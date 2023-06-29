cmake_minimum_required(VERSION 3.14...3.26)

# ---- Project ----

# Note: update this to your new project's name and version
project(
  dplang
  VERSION 1.0
  DESCRIPTION "DataPanel Language"
  LANGUAGES C
)

add_executable(dplang chunk.c compiler.c main.c memory.c scanner.c value.c vm.c object.c table.c hash.c parser.c builtins.c)
target_link_libraries(dplang m)

