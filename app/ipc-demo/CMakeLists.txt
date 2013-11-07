find_package(Boost 1.48 COMPONENTS serialization REQUIRED)

include_directories("${PROJECT_SOURCE_DIR}/lib/fles_core" "${PROJECT_SOURCE_DIR}/app/flesnet" "${PROJECT_SOURCE_DIR}/lib/fles_ipc")

include_directories(${Boost_INCLUDE_DIRS})

add_executable(pattern-check pattern-check.cpp)
target_link_libraries(pattern-check fles_ipc rt ${Boost_LIBRARIES})

add_executable(raw-storage raw-storage.cpp)
target_link_libraries(raw-storage fles_ipc rt ${Boost_LIBRARIES})
