
find_program(BASH_PROGRAM bash)

# By some reason, it did not work to put the tests in the tests directory
if(NOT BASH_PROGRAM)
	# non-fatal error
	message(WARNING "bash not found, tsa2msa system tests will not be run")
else()
	message(STATUS "bash found at ${BASH_PROGRAM}")
	message(STATUS "tsa2msa system tests will be run")
	add_test(
		NAME		tsa2msa_smokeTestVersion
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/smokeTestVersion.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	add_test(
		NAME		tsa2msa_smokeTestHelp
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/smokeTestHelp.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	add_test(
		NAME		tsa2msa_smokeTestWithMissingInput
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/smokeTestWithMissingInput.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
	# Create dir ${CMAKE_BINARY_DIR}/test/reference
	file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/test/reference)
	# copy ${CMAKE_PROJECT_SOURCE_DIR}/test/reference/example1.tsa
	#to ${CMAKE_BINARY_DIR}/test/reference/example1.tsa
	configure_file(
		${PROJECT_SOURCE_DIR}/test/reference/example1.tsa
		${CMAKE_BINARY_DIR}/test/reference/example1.tsa 
		COPYONLY)
	add_test(
		NAME		tsa2msa_smokeTestDryRunOption
		COMMAND		${BASH_PROGRAM} ${CMAKE_CURRENT_SOURCE_DIR}/smokeTestDryRunOption.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	)
endif()

