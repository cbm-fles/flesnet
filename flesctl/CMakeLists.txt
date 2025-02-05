# Update flesctl on mFLES (for now):
# cmake -E make_directory build_install_flesctl
# cmake -DCMAKE_INSTALL_PREFIX=/opt/fles -B build_install_flesctl
# sudo cmake -DCOMPONENT=flesctl -P build_install_flesctl/cmake_install.cmake

# New system
install(PROGRAMS flesctl-ng-wrapper DESTINATION bin RENAME flesctl-ng COMPONENT flesctl EXCLUDE_FROM_ALL)
install(PROGRAMS init_run.py fles_input.py DESTINATION flesctl-ng COMPONENT flesctl EXCLUDE_FROM_ALL)
install(FILES flesctl-ng.py flescfg.py DESTINATION flesctl-ng COMPONENT flesctl EXCLUDE_FROM_ALL)
