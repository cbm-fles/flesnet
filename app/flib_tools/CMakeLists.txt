# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(flib_status flib_status.cpp)
add_executable(flib_info flib_info.cpp)
add_executable(test_rf test_rf.cpp)
add_executable(en_readout en_readout.cpp)

target_link_libraries(flib_status flib-static pda)
target_link_libraries(flib_info flib-static pda)
target_link_libraries(test_rf flib-static)
target_link_libraries(en_readout flib-static)
