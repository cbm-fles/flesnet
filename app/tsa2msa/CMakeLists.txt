# Copyright 2024 Benjamin Kaiser <kaiser@zib.de>


list(APPEND SOURCES "${CMAKE_BINARY_DIR}/config/GitRevision.cpp")
list(APPEND HEADERS "${PROJECT_SOURCE_DIR}/config/GitRevision.hpp")

add_executable(tsa2msa main.cpp ${SOURCES} ${HEADERS})

target_include_directories(tsa2msa
  SYSTEM PUBLIC 	${Boost_INCLUDE_DIRS}
  PRIVATE 			${PROJECT_SOURCE_DIR}/config
)

target_link_libraries(tsa2msa
  ${Boost_LIBRARIES}
)

# Ensure warnings are enabled and standard is set:
target_compile_options(tsa2msa PRIVATE -Wall -Wextra -pedantic -std=c++17)

# Ensure we have -Werror in the build:
target_compile_options(tsa2msa PRIVATE -Werror)

enable_testing()
include(CTest)

# Print ${CMAKE_CURRENT_SOURCE_DIR} for debugging
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

find_program(BASH_PROGRAM bash)
if(NOT BASH_PROGRAM)
	# non-fatal error
	message(WARNING "bash not found, tsa2msa system tests will not be run")
else()
	add_test(
		NAME		tsa2msaSystemTest
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/tests/tsa2msaSystemTest.sh
	)
endif()
install(TARGETS tsa2msa DESTINATION bin)

