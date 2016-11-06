# Copyright 2015, 2016 Jan de Cuveland <cmail@cuveland.de>

include_directories(${PROJECT_SOURCE_DIR}/lib/fles_ipc
                    ${PROJECT_SOURCE_DIR}/lib/fles_core
                    ${PROJECT_SOURCE_DIR}/lib/logging)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(test_Timeslice test_Timeslice.cpp)
add_executable(test_Microslice test_Microslice.cpp)
add_executable(test_RingBuffer test_RingBuffer.cpp)
add_executable(test_Filter test_Filter.cpp)
add_executable(test_MicrosliceReceiver test_MicrosliceReceiver.cpp)

target_link_libraries(test_Timeslice fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_Microslice fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_RingBuffer fles_core ${Boost_LIBRARIES})
target_link_libraries(test_Filter fles_core ${Boost_LIBRARIES})
target_link_libraries(test_MicrosliceReceiver fles_core fles_ipc logging ${Boost_LIBRARIES})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(test_MicrosliceReceiver atomic pthread)
endif()

add_custom_command(TARGET test_Timeslice POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PROJECT_SOURCE_DIR}/test/reference/example1.tsa
                   $<TARGET_FILE_DIR:test_Timeslice>)
add_custom_command(TARGET test_Microslice POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PROJECT_SOURCE_DIR}/test/reference/example1.msa
                   $<TARGET_FILE_DIR:test_Microslice>)
add_custom_command(TARGET test_Filter POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PROJECT_SOURCE_DIR}/test/reference/example2.msa
                   $<TARGET_FILE_DIR:test_Filter>)

add_test(NAME test_Timeslice COMMAND test_Timeslice)
add_test(NAME test_Microslice COMMAND test_Microslice)
add_test(NAME test_RingBuffer COMMAND test_RingBuffer)
add_test(NAME test_Filter COMMAND test_Filter)
add_test(NAME test_MicrosliceReceiver COMMAND test_MicrosliceReceiver)

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
  add_test(NAME test_mstool
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_mstool.sh
           WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
