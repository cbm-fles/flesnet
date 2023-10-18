# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(msconsumer msconsumer.cpp)

target_compile_definitions(msconsumer PUBLIC BOOST_ALL_DYN_LINK)

target_link_libraries(msconsumer
    flib_ipc logging
    ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

if(NOT APPLE)
  target_link_libraries(msconsumer rt)
endif()

install(TARGETS msconsumer DESTINATION bin)
