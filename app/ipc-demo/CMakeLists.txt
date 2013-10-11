find_package(Boost 1.48 COMPONENTS serialization REQUIRED)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_core" "${PROJECT_SOURCE_DIR}/app/flesnet" "${PROJECT_SOURCE_DIR}/lib/fles_ipc")

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -pedantic -Wold-style-cast -Wfloat-equal -Woverloaded-virtual -Wundef -O3")

add_executable(pattern-check pattern-check.cpp)
target_link_libraries(pattern-check fles_ipc rt)

add_executable(raw-storage raw-storage.cpp)
target_link_libraries(raw-storage fles_ipc rt ${Boost_SERIALIZATION_LIBRARY})
