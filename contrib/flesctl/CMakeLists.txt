install(PROGRAMS fles_input DESTINATION bin COMPONENT flesctl EXCLUDE_FROM_ALL)
install(PROGRAMS flesctl-wrapper DESTINATION bin RENAME flesctl COMPONENT flesctl EXCLUDE_FROM_ALL)
install(PROGRAMS flesctl init_run DESTINATION flesctl COMPONENT flesctl EXCLUDE_FROM_ALL)
