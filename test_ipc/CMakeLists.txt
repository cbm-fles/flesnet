# Copyright 2014 Jan de Cuveland <cmail@cuveland.de>

find_package(Boost COMPONENTS system filesystem serialization unit_test_framework REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/src)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(test_fles_ipc test_Timeslice.cpp)

target_link_libraries(test_fles_ipc ${CMAKE_BINARY_DIR}/libfles_ipc.a ${Boost_LIBRARIES})

add_test(NAME test_fles_ipc COMMAND test_fles_ipc)
