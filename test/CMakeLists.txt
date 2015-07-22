# Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

include_directories(${PROJECT_SOURCE_DIR}/lib/fles_ipc/src
                    ${PROJECT_SOURCE_DIR}/lib/fles_core)

add_executable(test_RingBuffer test_RingBuffer.cpp)

target_link_libraries(test_RingBuffer fles_core ${Boost_LIBRARIES})

add_test(NAME test_RingBuffer COMMAND test_RingBuffer)
