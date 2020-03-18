# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(flib_status flib_status.cpp)
add_executable(flib_info flib_info.cpp)
add_executable(test_rf test_rf.cpp)
add_executable(en_readout en_readout.cpp)

target_link_libraries(flib_status flib pda fles_ipc ${Boost_LIBRARIES} ${CPPREST_LIBRARY} crypto)
target_link_libraries(flib_info flib pda)
target_link_libraries(test_rf flib)
target_link_libraries(en_readout flib)

install(TARGETS flib_status flib_info test_rf en_readout DESTINATION bin)
