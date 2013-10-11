file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

include_directories("${PROJECT_SOURCE_DIR}/app/flesnet" "${PROJECT_SOURCE_DIR}/lib/einhard")

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -pedantic -Wold-style-cast -Wfloat-equal -Woverloaded-virtual -Wundef -O3")

add_library(core ${LIB_SOURCES} ${LIB_HEADERS})
