file(GLOB APP_SOURCES *.cpp)
file(GLOB APP_HEADERS *.h)

set_property(DIRECTORY APPEND PROPERTY COMPILE_DEFINITIONS MINLOGLEVEL=2)

find_package(Boost 1.48 COMPONENTS program_options thread REQUIRED)

include_directories("${PROJECT_SOURCE_DIR}/app/flesnet" "${PROJECT_SOURCE_DIR}/lib/fles_core" "${PROJECT_SOURCE_DIR}/lib/einhard" "${PROJECT_SOURCE_DIR}/lib/fles_ipc" "${Boost_INCLUDE_DIRS}")

set(CMAKE_CXX_FLAGS "-std=c++11 -g -Wall -pedantic -Wold-style-cast -Wfloat-equal -Woverloaded-virtual -Wundef -O3")

add_executable(flesnet ${APP_SOURCES} ${APP_HEADERS})

target_link_libraries(flesnet fles_core flib ibverbs rdmacm rt numa ${Boost_LIBRARIES})
