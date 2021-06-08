# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(cri_info cri_info.cpp)
add_executable(cri_status cri_status.cpp)
add_executable(test_cri_rf test_cri_rf.cpp)
add_executable(cri_en_pgen cri_en_pgen.cpp)

target_link_libraries(cri_info cri pda)
target_link_libraries(cri_status cri pda fles_ipc ${Boost_LIBRARIES} ${CPPREST_LIBRARY} crypto)
target_link_libraries(test_cri_rf cri)
target_link_libraries(cri_en_pgen cri)

install(TARGETS cri_info DESTINATION bin)
install(TARGETS cri_status DESTINATION bin)
install(TARGETS cri_en_pgen DESTINATION bin)
