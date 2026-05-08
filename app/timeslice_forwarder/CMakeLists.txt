add_executable(timeslice_forwarder)

set(LIBFABRIC_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/libfabric_build/install_dir")
set(LIBFABRIC_INCLUDE_DIR "${LIBFABRIC_INSTALL_DIR}/include")
set(LIBFABRIC_LIB_DIR     "${LIBFABRIC_INSTALL_DIR}/lib")
set(LIBFABRIC_LIBRARY "${LIBFABRIC_LIB_DIR}/libfabric.so")

include(ExternalProject)
ExternalProject_Add(libfabric
    GIT_REPOSITORY    https://github.com/ofiwg/libfabric.git
    GIT_TAG           v2.5.1
    CONFIGURE_COMMAND ./autogen.sh COMMAND ./configure --prefix=${LIBFABRIC_INSTALL_DIR} --disable-sockets --disable-upd --disable-tcp --disable-opx --disable-psm2 --disable-psm3 --disable-rxm --disable-usnic --disable-shm --disable-efa --enable-verbs
    UPDATE_DISCONNECTED 1
    BUILD_IN_SOURCE 1
)

IF (DATA_FORWARDER_INSTALL_PATH)
    add_dependencies(timeslice_forwarder libfabric)
    message("Using installed data forwarder library at: ${DATA_FORWARDER_INSTALL_PATH}")
    set(DATA_FORWARDER_INSTALL_DIR "${DATA_FORWARDER_INSTALL_PATH}")
ELSE ()
    message("Pulling data forwarder lib from git")
    set(DATA_FORWARDER_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/data_forwarder_lib_build/install_dir")
    ExternalProject_Add(data_forwarder_lib
        GIT_REPOSITORY https://git.zib.de/ngreve/timeslice_forwarder.git
        GIT_TAG dev
        CMAKE_ARGS
            -DLIBFABRIC_INSTALL_PATH=${LIBFABRIC_INSTALL_DIR}
            -DCMAKE_INSTALL_PREFIX=${DATA_FORWARDER_INSTALL_DIR}
        UPDATE_DISCONNECTED 1
        BUILD_IN_SOURCE 1
    )
    add_dependencies(data_forwarder_lib libfabric)
    add_dependencies(timeslice_forwarder data_forwarder_lib)
ENDIF()

set(DATA_FORWARDER_INCLUDE_DIR "${DATA_FORWARDER_INSTALL_DIR}/include")
set(DATA_FORWARDER_LIB_DIR     "${DATA_FORWARDER_INSTALL_DIR}/lib")
set(DATA_FORWARDER_LIBRARY "${DATA_FORWARDER_LIB_DIR}/libdata_forwarder.a")

find_package(Boost 1.74.0 REQUIRED CONFIG COMPONENTS serialization program_options graph iostreams)
target_include_directories(timeslice_forwarder PRIVATE
    ./include
    ${LIBFABRIC_INCLUDE_DIR}
    ${PROJECT_SOURCE_DIR}/config
    ${DATA_FORWARDER_INCLUDE_DIR}
)

target_link_libraries(timeslice_forwarder PRIVATE
    ${DATA_FORWARDER_LIBRARY}
    fles_ipc
    fles_core
    flib_ipc
    logging
    crcutil
    ${Boost_LIBRARIES}
    ${LIBFABRIC_LIBRARY}
)

file(GLOB timeslice_forwarder_SRC
    "src/**/*.cpp"
    "src/*.cpp"
)

target_sources(timeslice_forwarder PRIVATE ${timeslice_forwarder_SRC})
