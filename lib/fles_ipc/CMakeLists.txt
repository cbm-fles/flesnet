# Copyright 2013, 2015, 2016 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

# The shared memory producer classes need std::format, including formatters
# that take std::format_context, which libc++ before version 19 rejects.
# Projects building fles_ipc with an older standard library (e.g., GCC 12)
# get it without them.
include(CheckCXXSourceCompiles)
block()
  set(CMAKE_CXX_STANDARD 23)
  check_cxx_source_compiles("
#include <format>
struct S {};
template <> struct std::formatter<S> {
  constexpr auto parse(std::format_parse_context& ctx) { return ctx.begin(); }
  auto format(const S&, std::format_context& ctx) const {
    return std::format_to(ctx.out(), \"s\");
  }
};
int main() { return static_cast<int>(std::format(\"{}{}\", 1, S{}).size()); }"
    FLES_IPC_HAS_STD_FORMAT)

  # libc++ before version 20 has std::jthread only with -fexperimental-library
  set(jthread_test_source "
#include <thread>
int main() { std::jthread t([] {}); }")
  check_cxx_source_compiles("${jthread_test_source}" FLES_IPC_HAS_JTHREAD)
  if(NOT FLES_IPC_HAS_JTHREAD)
    string(APPEND CMAKE_REQUIRED_FLAGS " -fexperimental-library")
    check_cxx_source_compiles("${jthread_test_source}"
      FLES_IPC_JTHREAD_NEEDS_EXPERIMENTAL_LIBRARY)
  endif()
endblock()
if(NOT FLES_IPC_HAS_STD_FORMAT)
  message(STATUS "=> No std::format, building fles_ipc without TimesliceShmBuffer and TimesliceShmSink.")
  list(REMOVE_ITEM LIB_SOURCES
       ${CMAKE_CURRENT_SOURCE_DIR}/TimesliceShmBuffer.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/TimesliceShmSink.cpp)
endif()

# Check if the Boost iostream was compiled with ZSTD (un)compression support
block()
  list(APPEND CMAKE_REQUIRED_INCLUDES ${Boost_INCLUDE_DIRS})
  list(APPEND CMAKE_REQUIRED_LIBRARIES ${Boost_IOSTREAMS_LIBRARY})
  check_cxx_source_compiles("
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/iostreams/filtering_stream.hpp>

int main(int argc, char *argv[])
{
  std::unique_ptr<boost::iostreams::filtering_istream> in_;
  in_ = std::make_unique<boost::iostreams::filtering_istream>();
  in_->push(boost::iostreams::zstd_decompressor());

  return 0;
}" BOOST_IOS_HAS_ZSTD)
endblock()

if(BOOST_IOS_HAS_ZSTD)
  message(STATUS "=> Boost::iostream with ZSTD filter found.")
else()
  message(STATUS "=> Boost::iostream does not support ZSTD filter.")
endif()

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})

# The headers used outside flesnet stay C++17; SubTimeslice.hpp,
# OptionValues.hpp and TimesliceShm*.hpp need C++23
target_compile_features(fles_ipc PUBLIC cxx_std_17 PRIVATE cxx_std_23)

target_compile_definitions(fles_ipc PUBLIC BOOST_ALL_DYN_LINK)
if(BOOST_IOS_HAS_ZSTD)
  target_compile_definitions(fles_ipc PUBLIC BOOST_IOS_HAS_ZSTD)
endif()

# Public, as AggregatingSource.hpp uses std::jthread in C++20 and later
if(FLES_IPC_JTHREAD_NEEDS_EXPERIMENTAL_LIBRARY)
  target_compile_options(fles_ipc PUBLIC -fexperimental-library)
endif()

target_include_directories(fles_ipc PUBLIC .)

target_include_directories(fles_ipc SYSTEM
  PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(fles_ipc
  PUBLIC shm_ipc
  PUBLIC zmq::cppzmq
  PUBLIC logging
  PUBLIC Boost::serialization
  PUBLIC Boost::iostreams
)

install(TARGETS fles_ipc DESTINATION lib COMPONENT dev)
install(FILES ${LIB_HEADERS} DESTINATION include COMPONENT dev)

add_library(fles::fles_ipc ALIAS fles_ipc)
