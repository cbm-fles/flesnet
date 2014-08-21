# Copies files if the don't exist in destination.
# Warns if source files are newer than destination files.

IF (NOT EXISTS ${CP_DEST})
 message("Copying ${CP_SRC} to ${CP_DEST}")
 EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E copy ${CP_SRC} ${CP_DEST})
ENDIF (NOT EXISTS ${CP_DEST})

IF (${CP_SRC} IS_NEWER_THAN ${CP_DEST})
 message("Consider updating your cfg file. ${CP_SRC} is newer than ${CP_DEST}.")
ENDIF (${CP_SRC} IS_NEWER_THAN ${CP_DEST})

