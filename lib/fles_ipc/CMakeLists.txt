# Copyright 2013, 2015, 2016 Jan de Cuveland <cuveland@compeng.uni-frankfurt.de>
# Copyright 2014 Dirk Hutter <hutter@compeng.uni-frankfurt.de>

file(GLOB LIB_SOURCES *.cpp)
file(GLOB LIB_HEADERS *.hpp)

#------------------------------------------------------------------------------#
set(OLD_CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS})
set(OLD_CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES})
set(OLD_CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES})

#set(CMAKE_REQUIRED_DEFINITIONS ${CMAKE_REQUIRED_DEFINITIONS} BOOST_ALL_DYN_LINK)
set(CMAKE_REQUIRED_INCLUDES ${CMAKE_REQUIRED_INCLUDES} ${Boost_INCLUDE_DIRS})
set(CMAKE_REQUIRED_LIBRARIES ${CMAKE_REQUIRED_LIBRARIES} ${Boost_IOSTREAMS_LIBRARY})

# Check if the Boost iostream was compiled with ZSTD (un)compression support
CHECK_CXX_SOURCE_COMPILES("
#include <boost/iostreams/filter/zstd.hpp>
#include <boost/iostreams/filtering_stream.hpp>

int main(int argc, char *argv[])
{
  std::unique_ptr<boost::iostreams::filtering_istream> in_;
  in_ = std::make_unique<boost::iostreams::filtering_istream>();
  in_->push(boost::iostreams::zstd_decompressor());

  return 0;
}" BOOST_IOS_HAS_ZSTD)

if(BOOST_IOS_HAS_ZSTD)
  message(STATUS "=> Boost::iostream with ZSTD filter found.")
else()
  message(STATUS "=> Boost::iostream does not support ZSTD filter.")
endif()

set(CMAKE_REQUIRED_DEFINITIONS ${OLD_CMAKE_REQUIRED_DEFINITIONS})
set(CMAKE_REQUIRED_INCLUDES ${OLD_CMAKE_REQUIRED_INCLUDES})
set(CMAKE_REQUIRED_LIBRARIES ${OLD_CMAKE_REQUIRED_LIBRARIES})
#------------------------------------------------------------------------------#

add_library(fles_ipc ${LIB_SOURCES} ${LIB_HEADERS})

target_compile_definitions(fles_ipc PUBLIC BOOST_ALL_DYN_LINK)
if(BOOST_IOS_HAS_ZSTD)
  target_compile_definitions(fles_ipc PUBLIC BOOST_IOS_HAS_ZSTD)
endif()

target_include_directories(fles_ipc PUBLIC .)

target_include_directories(fles_ipc SYSTEM
  PUBLIC ${Boost_INCLUDE_DIRS}
)

target_link_libraries(fles_ipc
  PUBLIC shm_ipc
  PUBLIC zmq::cppzmq
  PUBLIC logging
)
