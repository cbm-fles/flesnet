# Copyright 2026 Jan de Cuveland <cmail@cuveland.de>

add_executable(test_logging_fatal test_logging_fatal.cpp)
target_link_libraries(test_logging_fatal logging)

add_executable(test_logging_exit test_logging_exit.cpp)
target_link_libraries(test_logging_exit logging)

add_executable(test_logging_stall test_logging_stall.cpp)
target_link_libraries(test_logging_stall logging Threads::Threads)
add_test(NAME test_logging_stall COMMAND test_logging_stall)
add_test(NAME test_logging_stall_exit COMMAND test_logging_stall exit)
add_test(NAME test_logging_stall_fatal COMMAND test_logging_stall fatal)
set_tests_properties(test_logging_stall test_logging_stall_exit
                     test_logging_stall_fatal PROPERTIES TIMEOUT 60)

add_executable(test_logging_core test_logging_core.cpp)
target_compile_definitions(test_logging_core PRIVATE
  BOOST_TEST_DYN_LINK
  TEST_LOGGING_FATAL="$<TARGET_FILE:test_logging_fatal>"
  TEST_LOGGING_EXIT="$<TARGET_FILE:test_logging_exit>"
)
target_include_directories(test_logging_core SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(test_logging_core logging ${Boost_LIBRARIES})
add_test(NAME test_logging_core COMMAND test_logging_core)

# The same with {fmt} in place of <format>, as on systems without the latter.
# In a directory of its own, since both write files of the same names.
add_executable(test_logging_core_fmt test_logging_core.cpp)
set_target_properties(test_logging_core_fmt PROPERTIES
  OUTPUT_NAME test_logging_core
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fmt
)
target_compile_definitions(test_logging_core_fmt PRIVATE
  FLES_LOGGING_USE_FMT
  BOOST_TEST_DYN_LINK
  TEST_LOGGING_FATAL="$<TARGET_FILE:test_logging_fatal>"
  TEST_LOGGING_EXIT="$<TARGET_FILE:test_logging_exit>"
)
target_include_directories(test_logging_core_fmt SYSTEM PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(test_logging_core_fmt logging ${Boost_LIBRARIES})
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  # False positives in {fmt} 9 with newer compilers
  target_compile_options(test_logging_core_fmt PRIVATE
    -Wno-array-bounds -Wno-stringop-overflow)
endif()
add_test(NAME test_logging_core_fmt COMMAND test_logging_core_fmt
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fmt)

# One logging instance across shared libraries
add_library(testlog_a SHARED testlog_a.cpp)
target_link_libraries(testlog_a logging)

add_library(testlog_b SHARED testlog_b.cpp)
set_target_properties(testlog_b PROPERTIES CXX_VISIBILITY_PRESET hidden)
target_link_libraries(testlog_b logging)

add_executable(test_logging_linked test_logging_dlopen.cpp)
target_compile_definitions(test_logging_linked PRIVATE TESTLOG_LINKED)
target_link_libraries(test_logging_linked testlog_a testlog_b Threads::Threads)

# Deliberately not linked to logging, so that it is not in the global scope
add_executable(test_logging_dlopen test_logging_dlopen.cpp)
target_compile_definitions(test_logging_dlopen PRIVATE
  TESTLOG_A="$<TARGET_FILE:testlog_a>"
  TESTLOG_B="$<TARGET_FILE:testlog_b>"
)
target_link_libraries(test_logging_dlopen ${CMAKE_DL_LIBS} Threads::Threads)
add_dependencies(test_logging_dlopen testlog_a testlog_b)

add_test(NAME test_logging_linked
         COMMAND test_logging_linked linked test_logging_linked.json)
add_test(NAME test_logging_dlopen_global
         COMMAND test_logging_dlopen global test_logging_dlopen_global.json)
add_test(NAME test_logging_dlopen_local
         COMMAND test_logging_dlopen local test_logging_dlopen_local.json)
add_test(NAME test_logging_dlopen_local_late
         COMMAND test_logging_dlopen local-late test_logging_dlopen_local_late.json)

add_executable(test_logging_unload test_logging_unload.cpp)
target_compile_definitions(test_logging_unload PRIVATE
  TESTLOG_A="$<TARGET_FILE:testlog_a>"
)
target_link_libraries(test_logging_unload logging ${CMAKE_DL_LIBS})
add_dependencies(test_logging_unload testlog_a)
add_test(NAME test_logging_unload
         COMMAND test_logging_unload test_logging_unload.json)

# Not a test; run manually
add_executable(bench_logging bench_logging.cpp)
target_link_libraries(bench_logging logging Threads::Threads)
