# Copyright 2015 Jan de Cuveland <cmail@cuveland.de>

include_directories(${PROJECT_SOURCE_DIR}/lib/fles_ipc/src
                    ${PROJECT_SOURCE_DIR}/lib/fles_core
                    ${PROJECT_SOURCE_DIR}/lib/logging)

add_executable(test_RingBuffer test_RingBuffer.cpp)
add_executable(test_Filter test_Filter.cpp)
add_executable(test_MicrosliceReceiver test_MicrosliceReceiver.cpp)

target_link_libraries(test_RingBuffer fles_core ${Boost_LIBRARIES})
target_link_libraries(test_Filter fles_core ${Boost_LIBRARIES})
target_link_libraries(test_MicrosliceReceiver fles_core fles_ipc logging ${Boost_LIBRARIES})
if(LIBNUMA AND USE_LIBNUMA)
  target_link_libraries(test_MicrosliceReceiver numa)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(test_MicrosliceReceiver atomic pthread)
endif()

add_custom_command(TARGET test_Filter POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                                   ${PROJECT_SOURCE_DIR}/test/reference/example1.msa
                                   $<TARGET_FILE_DIR:test_Filter>)

add_test(NAME test_RingBuffer COMMAND test_RingBuffer)
add_test(NAME test_Filter COMMAND test_Filter)
add_test(NAME test_MicrosliceReceiver COMMAND test_MicrosliceReceiver)
