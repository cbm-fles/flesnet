add_executable(timeslice_forwarder)

ExternalProject_Add(timeslice_forwarder_lib
    GIT_REPOSITORY    https://git.zib.de/ngreve/timeslice_forwarder.git
    GIT_TAG dev
    CMAKE_ARGS
        -DUSE_EXTERNAL_LIBFABRIC=True
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/timeslice_forwarder_lib_build/install_dir
    UPDATE_DISCONNECTED 1
    BUILD_IN_SOURCE 1
)
add_dependencies(timeslice_forwarder timeslice_forwarder_lib)

SET(USE_EXTERNAL_LIBFABRIC True)
include(FetchContent)
FetchContent_Declare(timeslice_forwarder_lib
    GIT_REPOSITORY    https://git.zib.de/ngreve/timeslice_forwarder.git
    GIT_TAG dev
    CMAKE_ARGS
        -DUSE_EXTERNAL_LIBFABRIC=True
        -DCMAKE_INSTALL_PREFIX=${CMAKE_CURRENT_BINARY_DIR}/timeslice_forwarder_lib_build/install_dir
)
FetchContent_MakeAvailable(timeslice_forwarder_lib)

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
