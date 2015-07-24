# Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

include_directories(${PROJECT_SOURCE_DIR}/lib/fles_ipc/src
                    ${PROJECT_SOURCE_DIR}/lib/fles_core
                    ${PROJECT_SOURCE_DIR}/lib/logging)

add_executable(test_RingBuffer test_RingBuffer.cpp)
add_executable(test_MicrosliceReceiver test_MicrosliceReceiver.cpp)

target_link_libraries(test_RingBuffer fles_core ${Boost_LIBRARIES})
target_link_libraries(test_MicrosliceReceiver fles_core fles_ipc logging ${Boost_LIBRARIES})
if(LIBNUMA AND USE_LIBNUMA)
  target_link_libraries(test_MicrosliceReceiver numa)
endif()

add_test(NAME test_RingBuffer COMMAND test_RingBuffer)
add_test(NAME test_MicrosliceReceiver COMMAND test_MicrosliceReceiver)
