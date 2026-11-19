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
  CXX_VISIBILITY_PRESET hidden
)
# Next to the applications (see CMAKE_BUILD_RPATH), unless a parent project
# has its own library directory
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
  set_target_properties(logging PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
  )
endif()

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
