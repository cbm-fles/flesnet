
SET(TIMESLICE_FORWARDER_LIB_INCLUDES /home/htc/ngreve/prog/timeslice_forwarder/build/install_dir/include/)
find_library(TIMESLICE_FORWARDER_LIB NAMES data_forwarder PATHS /home/htc/ngreve/prog/timeslice_forwarder/build/install_dir/lib/ NO_DEFAULT_PATH)

SET(LIBFABRIC_INCLUDE_DIR /home/htc/ngreve/prog/libfabric/install_dir/include/)
find_library(LIBFABRIC_LIBRARY NAMES fabric PATHS /home/htc/ngreve/prog/libfabric/install_dir/lib/ NO_DEFAULT_PATH)

find_package(Boost 1.74.0 REQUIRED COMPONENTS serialization program_options graph iostreams)

add_executable(timeslice_forwarder)
target_include_directories(timeslice_forwarder PRIVATE "${PROJECT_SOURCE_DIR}/config")
target_include_directories(timeslice_forwarder PRIVATE
    ./include
    ${PROJECT_SOURCE_DIR}/config
    ${TIMESLICE_FORWARDER_LIB_INCLUDES}
)

target_link_libraries(timeslice_forwarder PRIVATE ${TIMESLICE_FORWARDER_LIB} fles_ipc fles_core flib_ipc logging crcutil ${Boost_LIBRARIES} ${LIBFABRIC_LIBRARY})

file(GLOB timeslice_forwarder_SRC
    "src/**/*.cpp"
    "src/*.cpp"
)

target_sources(timeslice_forwarder PRIVATE ${timeslice_forwarder_SRC})
