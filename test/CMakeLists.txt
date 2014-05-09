# Copyright 2014 Jan de Cuveland <cmail@cuveland.de>

find_package(Boost COMPONENTS system filesystem unit_test_framework REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/lib/fles_ipc)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(test_fles_ipc fles_ipc/test_Timeslice.cpp)

target_link_libraries(test_fles_ipc fles_ipc ${Boost_LIBRARIES})

add_test(NAME test_fles_ipc COMMAND test_fles_ipc)
