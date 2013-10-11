file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.h)

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -pedantic -Wold-style-cast -Wfloat-equal -Woverloaded-virtual -Wundef -O3")

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})
