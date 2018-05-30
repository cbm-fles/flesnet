# Copyright 2017 Dirk Hutter <cmail@dirk-hutter.de>

find_path(CPPREST_INCLUDE_DIR cpprest/http_client.h)
find_library(CPPREST_LIBRARY cpprest)
#set(CPPREST_INCLUDE_DIR "/home/reinhardt/src/cpprest_install/include/cpprest")
#set(CPPREST_LIBRARY "/home/reinhardt/src/cpprest_install/lib/cpprest")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(CPPREST REQUIRED_VARS CPPREST_LIBRARY CPPREST_INCLUDE_DIR)
