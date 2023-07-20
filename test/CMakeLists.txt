include_directories(${CMAKE_CURRENT_LIST_DIR}/..)
add_subdirectory(runtime)
add_subdirectory(hash)
add_subdirectory(util)
add_subdirectory(value)
add_subdirectory(scanner)
add_subdirectory(table)

add_test(NAME unexecuted_tests COMMAND ${CMAKE_CURRENT_LIST_DIR}/unused-tests.sh ${CMAKE_CURRENT_LIST_DIR})
