# Copyright 2016 Thorsten Schuett <schuett@zib.de> Updated by Farouk Salem <salem@zib.de>

file(GLOB LIB_SOURCES *.cpp providers/*.cpp dfs/*.cpp)
file(GLOB LIB_HEADERS *.hpp providers/*.hpp dfs/*.hpp)

add_library(fles_libfabric ${LIB_SOURCES} ${LIB_HEADERS})

target_compile_options(fles_libfabric
  PRIVATE -Wno-unknown-warning-option
  PRIVATE -Wno-unused-but-set-variable
  PRIVATE -Wno-unused-parameter
  PRIVATE -Wno-unused-variable
)

target_include_directories(fles_libfabric PUBLIC .)

target_include_directories(fles_libfabric SYSTEM
  PUBLIC ${LIBFABRIC_INCLUDE_DIR}
  PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(fles_libfabric
  PUBLIC fles_ipc
  PUBLIC fles_core
  PUBLIC logging
  PUBLIC ${LIBFABRIC_LIBRARY}
)
