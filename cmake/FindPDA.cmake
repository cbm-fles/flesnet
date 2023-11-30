# Copyright 2016 Jan de Cuveland <cmail@cuveland.de>

find_path(
        PDA_INCLUDE_DIR pda.h
        HINTS ${PDA_PREFIX_PATH}/include)

find_library(
        PDA_LIBRARY pda
        HINTS ${PDA_PREFIX_PATH}/lib)

find_program(PDA_CONFIG
        NAMES pda-config
        HINTS ${PDA_PREFIX_PATH}/bin)

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
