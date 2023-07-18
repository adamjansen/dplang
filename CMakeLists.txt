cmake_minimum_required(VERSION 3.14...3.26)

file(READ "version.txt" ver)
file(READ   "version.txt"   ver)
string(REGEX MATCH "VERSION_MAJOR[\t ]+([0-9]+)" _ ${ver})
set(ver_major ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_MINOR[\t ]+([0-9]+)" _ ${ver})
set(ver_minor ${CMAKE_MATCH_1})
string(REGEX MATCH "VERSION_PATCH[\t ]+([0-9]+)" _ ${ver})
set(ver_patch ${CMAKE_MATCH_1})


project(
  dplang
  VERSION "${ver_major}.${ver_minor}.${ver_patch}"
  DESCRIPTION "DataPanel Language"
  LANGUAGES C
)


set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "")
include(CTest)
include(ProcessorCount)

ProcessorCount(NPROC)

include_directories(
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_BINARY_DIR}
)

configure_file(dplang.h.in dplang.h)

add_subdirectory(external)

option(CODE_COVERAGE "Measure code coverage" ON)
option(BUILD_TESTS "Build the test suite" ON)

if (BUILD_TESTS)
  enable_testing()
  if (CODE_COVERAGE)
    include(CodeCoverage)
    append_coverage_compiler_flags()
    setup_target_for_coverage_gcovr_xml(
      NAME coverage
      EXECUTABLE ctest -j${NPROC}
      EXCLUDE "test/*"
    )
    find_program(pycobertura pycobertura)
    if(NOT pycobertura MATCHES "NOTFOUND")
      set (PYCOBERTURA "${pycobertura}"
          "show"
          "--source" "${CMAKE_CURRENT_SOURCE_DIR}"
          "--output" "coverage.html"
          "--format=html"
          "coverage.xml"
      )
      add_custom_target(coverage_html
          COMMAND ${PYCOBERTURA}
          DEPENDS coverage
          WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
          COMMENT "Preparing HTML report"
          VERBATIM)
      endif()
  endif()
  add_subdirectory(test)
endif()


find_program(CLANG_TIDY_EXE NAMES "clang-tidy" REQUIRED)
set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" "-checks=-*,bugprone-*,portability-*,readability-*,-readability-identifier-length,performance-*,misc-*,clang-analyzer-*,-clang-analyzer-security*,-readability-else-after-return,-bugprone-branch-clone")

set(CMAKE_C_FLAGS "-Wall -Wextra -pedantic")
set(CMAKE_C_FLAGS_RELEASE "-O3")


add_library(dplanglib STATIC chunk.c compiler.c memory.c scanner.c value.c vm.c object.c table.c hash.c parser.c builtins.c)

add_executable(dplang chunk.c compiler.c main.c memory.c scanner.c value.c vm.c object.c table.c hash.c parser.c builtins.c)
target_link_libraries(dplang m dplanglib)

set_target_properties(dplang PROPERTIES C_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
