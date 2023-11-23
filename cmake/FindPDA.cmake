# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

set(_PDA_PREFIX /opt/pda/${PDA_FIND_VERSION})

find_path(
        PDA_INCLUDE_DIR pda.h
        PATHS pda/include ${_PDA_PREFIX}/include)

find_library(
        PDA_LIBRARY pda
        PATHS pda/lib ${_PDA_PREFIX}/lib)

find_program(PDA_CONFIG
        NAMES pda-config
        PATHS pda/bin ${_PDA_PREFIX}/bin)

if(PDA_CONFIG)
execute_process(
        COMMAND ${PDA_CONFIG} "--version"
        OUTPUT_VARIABLE PDA_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PDA
        REQUIRED_VARS PDA_LIBRARY PDA_INCLUDE_DIR
        VERSION_VAR PDA_VERSION)
