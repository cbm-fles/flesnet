# Copyright 2015, 2016, 2020 Jan de Cuveland <cmail@cuveland.de>

add_executable(test_System test_System.cpp)
add_executable(test_Utility test_Utility.cpp)
add_executable(test_Timeslice test_Timeslice.cpp)
add_executable(test_Archive test_Archive.cpp)
add_executable(test_TimesliceAutoSource test_TimesliceAutoSource.cpp)
add_executable(test_TimesliceShm test_TimesliceShm.cpp)
add_executable(test_Microslice test_Microslice.cpp)
add_executable(test_logging test_logging.cpp)
add_executable(test_ItemProtocol test_ItemProtocol.cpp)

target_compile_definitions(test_System PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Utility PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Timeslice PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Archive PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_TimesliceAutoSource PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_TimesliceShm PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_Microslice PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_logging PUBLIC BOOST_TEST_DYN_LINK)
target_compile_definitions(test_ItemProtocol PUBLIC BOOST_TEST_DYN_LINK)

target_include_directories(test_System SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Utility SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Timeslice SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Archive SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_TimesliceAutoSource SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_TimesliceShm SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_Microslice SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_logging SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})
target_include_directories(test_ItemProtocol SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(test_System fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_Utility fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_Timeslice fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_Archive fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_TimesliceAutoSource fles_ipc ${Boost_LIBRARIES})
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(test_TimesliceAutoSource rt)
endif()
target_link_libraries(test_TimesliceShm fles_ipc logging ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_Microslice fles_ipc ${Boost_LIBRARIES})
target_link_libraries(test_logging logging ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(test_ItemProtocol shm_ipc logging ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

# For TimesliceShmBuffer.hpp
target_compile_features(test_TimesliceShm PRIVATE cxx_std_23)
# Projects like cbmroot use these headers from C++17
set_target_properties(test_TimesliceAutoSource PROPERTIES CXX_STANDARD 17)

if(APPLE)
  target_link_directories(test_System PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_Utility PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_Timeslice PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_Archive PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_TimesliceAutoSource PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_TimesliceShm PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_Microslice PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_logging PRIVATE ${ZSTD_LIB_DIR})
  target_link_directories(test_ItemProtocol PRIVATE ${ZSTD_LIB_DIR})
endif()

add_custom_command(TARGET test_Timeslice POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PROJECT_SOURCE_DIR}/test/reference/example1.tsa
                   $<TARGET_FILE_DIR:test_Timeslice>)
add_custom_command(TARGET test_TimesliceShm POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PROJECT_SOURCE_DIR}/test/reference/example1.tsa
                   $<TARGET_FILE_DIR:test_TimesliceShm>)
add_custom_command(TARGET test_Microslice POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PROJECT_SOURCE_DIR}/test/reference/example1.msa
                   $<TARGET_FILE_DIR:test_Microslice>)
add_custom_command(TARGET test_Microslice POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                   ${PROJECT_SOURCE_DIR}/test/reference/example2.msa
                   $<TARGET_FILE_DIR:test_Microslice>)

add_test(NAME test_System COMMAND test_System)
add_test(NAME test_Utility COMMAND test_Utility)
add_test(NAME test_Timeslice COMMAND test_Timeslice)
add_test(NAME test_Archive COMMAND test_Archive)
add_test(NAME test_TimesliceAutoSource COMMAND test_TimesliceAutoSource)
add_test(NAME test_TimesliceShm COMMAND test_TimesliceShm)
add_test(NAME test_Microslice COMMAND test_Microslice)
add_test(NAME test_logging COMMAND test_logging)
add_test(NAME test_ItemProtocol COMMAND test_ItemProtocol)
set_tests_properties(test_ItemProtocol PROPERTIES TIMEOUT 120)

find_program(BASH_PROGRAM bash)
if(BASH_PROGRAM)
  add_test(NAME test_mstool
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_mstool.sh
           WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  add_test(NAME test_shm_stream
           COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_shm_stream.sh
           WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
  set_tests_properties(test_shm_stream PROPERTIES TIMEOUT 180)
  if (USE_PDA)
    if (NOT APPLE)
      add_test(NAME test_with_pda
             COMMAND ${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/test_with_pda.sh
             WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    endif()
  endif()
endif()

add_subdirectory(shm_ipc)
