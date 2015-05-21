# Copyright 2014 Jan de Cuveland <cmail@cuveland.de>

include_directories(${PROJECT_SOURCE_DIR}/src)

add_definitions(-DBOOST_TEST_DYN_LINK)

add_executable(test_Timeslice test_Timeslice.cpp)
add_executable(test_Microslice test_Microslice.cpp)

target_link_libraries(test_Timeslice ${CMAKE_BINARY_DIR}/libfles_ipc.a ${Boost_LIBRARIES})
target_link_libraries(test_Microslice ${CMAKE_BINARY_DIR}/libfles_ipc.a ${Boost_LIBRARIES})

add_custom_command(TARGET test_Timeslice POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
				   ${CMAKE_SOURCE_DIR}/test/reference/example1.tsa
				   $<TARGET_FILE_DIR:test_Timeslice>)
add_custom_command(TARGET test_Microslice POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
				   ${CMAKE_SOURCE_DIR}/test/reference/example1.msa
				   $<TARGET_FILE_DIR:test_Microslice>)

add_test(NAME test_Timeslice COMMAND test_Timeslice)
add_test(NAME test_Microslice COMMAND test_Microslice)
