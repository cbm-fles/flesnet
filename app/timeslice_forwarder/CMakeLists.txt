add_executable(timeslice_forwarder)

set(LIBFABRIC_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/libfabric_build/install_dir")
set(LIBFABRIC_INCLUDE_DIR "${LIBFABRIC_INSTALL_DIR}/include")
set(LIBFABRIC_LIB_DIR     "${LIBFABRIC_INSTALL_DIR}/lib")
set(LIBFABRIC_LIBRARY "${LIBFABRIC_LIB_DIR}/libfabric.so")

include(ExternalProject)
ExternalProject_Add(libfabric
    GIT_REPOSITORY    https://github.com/ofiwg/libfabric.git
    GIT_TAG           v2.5.1
    CONFIGURE_COMMAND ./autogen.sh COMMAND ./configure --prefix=${LIBFABRIC_INSTALL_DIR}
    UPDATE_DISCONNECTED 1
    BUILD_IN_SOURCE 1
)

ExternalProject_Add(timeslice_forwarder_lib
    GIT_REPOSITORY    https://git.zib.de/ngreve/timeslice_forwarder.git
    GIT_TAG dev
    CMAKE_ARGS
        -DLIBFABRIC_INSTALL_PATH=${LIBFABRIC_INSTALL_DIR}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/timeslice_forwarder_lib_build/install_dir
    UPDATE_DISCONNECTED 1
    BUILD_IN_SOURCE 1
)
add_dependencies(timeslice_forwarder_lib libfabric)
add_dependencies(timeslice_forwarder timeslice_forwarder_lib)

find_package(Boost 1.74.0 REQUIRED CONFIG COMPONENTS serialization program_options graph iostreams)
target_include_directories(timeslice_forwarder PRIVATE
    ./include
    ${LIBFABRIC_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_BINARY_DIR}/timeslice_forwarder_lib_build/install_dir/include
)

target_link_libraries(timeslice_forwarder PRIVATE  ${CMAKE_CURRENT_BINARY_DIR}/timeslice_forwarder_lib_build/install_dir/lib/libdata_forwarder.a fles_ipc fles_core flib_ipc logging crcutil ${Boost_LIBRARIES} ${LIBFABRIC_LIBRARY})
file(GLOB timeslice_forwarder_SRC
    "src/**/*.cpp"
    "src/*.cpp"
)

get_cmake_property(_variableNames VARIABLES)
list (SORT _variableNames)
foreach (_variableName ${_variableNames})
    message(STATUS "${_variableName}=${${_variableName}}")
endforeach()

target_sources(timeslice_forwarder PRIVATE ${timeslice_forwarder_SRC})
