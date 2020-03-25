# Copyright 2015, 2016 Jan de Cuveland <cmail@cuveland.de>

add_executable(test_System test_System.cpp)
add_executable(test_Timeslice test_Timeslice.cpp)
add_executable(test_Archive test_Archive.cpp)
add_executable(test_TimesliceMultiInputArchive test_TimesliceMultiInputArchive.cpp)
add_executable(test_Microslice test_Microslice.cpp)
add_executable(test_RingBuffer test_RingBuffer.cpp)
add_executable(test_Filter test_Filter.cpp)
add_executable(test_MicrosliceReceiver test_MicrosliceReceiver.cpp)
add_executable(test_logging test_logging.cpp)

target_compile_definitions(test_System PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Timeslice PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Archive PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_TimesliceMultiInputArchive PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Microslice PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_RingBuffer PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Filter PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_MicrosliceReceiver PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_logging PUBLIC BOOST_TEST_DYN_LINK)

target_include_directories(test_System SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Timeslice SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Archive SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_TimesliceMultiInputArchive SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Microslice SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_RingBuffer SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Filter SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_MicrosliceReceiver SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_logging SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(test_System fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_Timeslice fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_Archive fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_TimesliceMultiInputArchive fles_ipc logging ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_Microslice fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_RingBuffer fles_core ${Boost_LIBRARIES})
target_link_libraries(test_Filter fles_core ${Boost_LIBRARIES})
target_link_libraries(test_MicrosliceReceiver fles_core fles_ipc logging ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(test_MicrosliceReceiver atomic)
endif()
target_link_libraries(test_logging logging ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

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

add_test(NAME test_System COMMAND test_System)
add_test(NAME test_Timeslice COMMAND test_Timeslice)
add_test(NAME test_Archive COMMAND test_Timeslice)
add_test(NAME test_TimesliceMultiInputArchive COMMAND test_TimesliceMultiInputArchive)
add_test(NAME test_Microslice COMMAND test_Microslice)
add_test(NAME test_RingBuffer COMMAND test_RingBuffer)
add_test(NAME test_Filter COMMAND test_Filter)
add_test(NAME test_MicrosliceReceiver COMMAND test_MicrosliceReceiver)
add_test(NAME test_logging COMMAND test_logging)

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
  add_test(NAME test_mstool
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_mstool.sh
           WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME test_with_pda
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_with_pda.sh
           WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endif()
