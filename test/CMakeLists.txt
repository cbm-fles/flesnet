# Copyright 2014 Jan de Cuveland <cmail@cuveland.de>

find_package(Boost COMPONENTS system filesystem serialization unit_test_framework REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/lib/fles_ipc/src)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(test_fles_ipc fles_ipc/test_Timeslice.cpp)

target_link_libraries(test_fles_ipc ${CMAKE_BINARY_DIR}/fles_ipc/src/fles_ipc-build/libfles_ipc.a ${Boost_LIBRARIES})

add_test(NAME test_fles_ipc COMMAND test_fles_ipc)
