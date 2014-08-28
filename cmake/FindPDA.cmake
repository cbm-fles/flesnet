set(REQUIRED_PDA_VERSION ${PDA_FIND_VERSION})

EXECUTE_PROCESS(
  COMMAND /opt/pda/${REQUIRED_PDA_VERSION}/bin/pda-config --include
  RESULT_VARIABLE PDA_RETURN
  OUTPUT_VARIABLE PDA_INCLUDE_DIR
  )
IF( PDA_RETURN )
  MESSAGE( STATUS "Failed to run pda-config because it is not in the path. ")
  MESSAGE( STATUS "Seriously, have you ever even tried to install the PDA library? ")
  MESSAGE( FATAL_ERROR "ABORT!!!")
ENDIF()
MESSAGE(STATUS "pda include         = ${PDA_INCLUDE_DIR}")
include_directories ( ${PDA_INCLUDE_DIR}  )

EXECUTE_PROCESS(
  COMMAND /opt/pda/${REQUIRED_PDA_VERSION}/bin/pda-config --version
  RESULT_VARIABLE ret
  OUTPUT_VARIABLE PDA_VERSION
  )
MESSAGE(STATUS "pda found VERSION   = ${PDA_VERSION}")
MESSAGE(STATUS "pda needed VERSION  = ${REQUIRED_PDA_VERSION}")

EXECUTE_PROCESS(
  COMMAND /opt/pda/${REQUIRED_PDA_VERSION}/bin/pda-config --ldlibrarypath
  RESULT_VARIABLE ret
  OUTPUT_VARIABLE PDA_LD_LIBRARY_PATH
  )
MESSAGE(STATUS "pda LD_LIBRARY_PATH = ${PDA_LD_LIBRARY_PATH}")

find_library(PDA pda PATHS ${PDA_LD_LIBRARY_PATH})
set(EXTRA_LIBS ${EXTRA_LIBS} ${PDA})
