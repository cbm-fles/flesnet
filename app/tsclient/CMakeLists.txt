# Copyright 2013 Jan de Cuveland <cmail@cuveland.de>

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.h)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_ipc")

add_executable(tsclient ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(tsclient fles_ipc ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})
if(UNIX AND NOT APPLE)
    target_link_libraries(tsclient rt)
endif()
