# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(cri_info cri_info.cpp)
add_executable(cri_status cri_status.cpp)
add_executable(cri_test_rf cri_test_rf.cpp)
add_executable(cri_en_pgen cri_en_pgen.cpp)

target_link_libraries(cri_info cri pda)
target_link_libraries(cri_status cri pda fles_ipc ${Boost_LIBRARIES} monitoring)
target_link_libraries(cri_test_rf cri)
target_link_libraries(cri_en_pgen cri)

install(TARGETS cri_info DESTINATION bin)
install(TARGETS cri_status DESTINATION bin)
install(TARGETS cri_en_pgen DESTINATION bin)
