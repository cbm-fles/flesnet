add_custom_target(lib_control
 COMMAND . ./config.sh && make
 WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/ctrl
 COMMENT "Updating control libraries in ctrl/"
 VERBATIM)

