# Copyright 2024 Benjamin Kaiser <kaiser@zib.de>


list(APPEND SOURCES "${CMAKE_BINARY_DIR}/config/GitRevision.cpp")
list(APPEND HEADERS "${PROJECT_SOURCE_DIR}/config/GitRevision.hpp")


list(APPEND SOURCES msaWriter.cpp)
list(APPEND HEADERS msaWriter.hpp)

list(APPEND SOURCES msaValidator.cpp)
list(APPEND HEADERS msaValidator.hpp)

list(APPEND SOURCES tsaReader.cpp)
list(APPEND HEADERS tsaReader.hpp)

list(APPEND SOURCES tsaValidator.cpp)
list(APPEND HEADERS tsaValidator.hpp)

list(APPEND SOURCES utils.cpp)
list(APPEND HEADERS utils.hpp)

add_executable(tsa2msa main.cpp ${SOURCES} ${HEADERS})

target_include_directories(tsa2msa
  SYSTEM PUBLIC		${Boost_INCLUDE_DIRS}
  PRIVATE 			${PROJECT_SOURCE_DIR}/config
  PUBLIC			${PROJECT_SOURCE_DIR}
)

# For lib/fles_ipc/TimesliceAutoSource.o:
target_link_libraries(tsa2msa
  fles_ipc
  ${Boost_LIBRARIES}
  rt  # is this really needed?
)

# Ensure warnings are enabled and standard is set:
target_compile_options(tsa2msa PRIVATE -Wall -Wextra -pedantic -std=c++17)

# Ensure we have -Werror in the build:
target_compile_options(tsa2msa PRIVATE -Werror)

message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)

install(TARGETS tsa2msa DESTINATION bin)


