# Copyright 2013-2014, 2016, 2026 Jan de Cuveland <cmail@cuveland.de>

# Always a shared library: it holds the process-wide queue and writer thread,
# which must exist only once even when several shared libraries use logging.
add_library(logging SHARED
  log.cpp log.hpp log_short.hpp log_program_options.hpp)

# SOVERSION: to be increased when the interface in log.hpp changes in a way
# that code built against the previous one does not work with.
set_target_properties(logging PROPERTIES
  OUTPUT_NAME fles_logging
  SOVERSION 1
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
  CXX_VISIBILITY_PRESET hidden
)

target_compile_features(logging PUBLIC cxx_std_17)

target_include_directories(logging PUBLIC .)

target_link_libraries(logging PUBLIC Threads::Threads)

# The format-style macros in log.hpp use {fmt} in code that is built without
# std::format, which depends on the C++ standard of the code including it.
target_link_libraries(logging INTERFACE fmt::fmt)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  # Stay mapped on dlclose: the writer thread may still use our code.
  target_link_options(logging PRIVATE -Wl,-z,nodelete)
endif()

# The applications need the shared library at run time, so it is not part of
# the dev component.
install(TARGETS logging LIBRARY DESTINATION lib)
install(FILES log.hpp log_short.hpp log_program_options.hpp
  DESTINATION include COMPONENT dev)

add_library(fles::logging ALIAS logging)
