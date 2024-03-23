
find_program(BASH_PROGRAM bash)

# By some reason, it did not work to put the tests in the tests directory
if(NOT BASH_PROGRAM)
	# non-fatal error
	message(WARNING "bash not found, tsa2msa system tests will not be run")
else()
	message(STATUS "bash found at ${BASH_PROGRAM}")
	message(STATUS "tsa2msa system tests will be run")
	add_test(
		NAME		smokeTestVersion
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/smokeTestVersion.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	add_test(
		NAME		smokeTestHelp
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/smokeTestHelp.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	add_test(
		NAME		smokeTestNoInput
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/smokeTestNoInput.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
endif()

