# Copyright 2014 Dirk Hutter, Jan de Cuveland

file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.hpp)

include_directories("${PROJECT_SOURCE_DIR}/lib/logging"
                    "${PROJECT_SOURCE_DIR}/lib/flib"
                    "${PROJECT_SOURCE_DIR}/lib")

add_executable(flib_server ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(flib_server flib logging ${Boost_LIBRARIES} rt)
