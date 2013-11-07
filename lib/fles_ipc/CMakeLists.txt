file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})
