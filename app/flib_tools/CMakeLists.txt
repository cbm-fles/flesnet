# Copyright 2014 Dirk Hutter, Jan de Cuveland

include_directories(
        "${PROJECT_SOURCE_DIR}/lib/flib"
        "${PROJECT_SOURCE_DIR}/lib"
        "${PROJECT_SOURCE_DIR}/lib/logging")

add_executable(flib_status flib_status.cpp)
add_executable(test_rf test_rf.cpp)
add_executable(en_readout en_readout.cpp)
#add_executable(simple_server_2 simple_server.cpp)

target_link_libraries(flib_status flib-static)
target_link_libraries(test_rf flib-static)
target_link_libraries(en_readout flib-static)
#target_link_libraries(simple_server_2 logging flib-static ${Boost_LIBRARIES})
#if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
#    target_link_libraries(simple_server_2 pthread)
#endif()

