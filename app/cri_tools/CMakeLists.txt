# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(cri_info cri_info.cpp)

target_link_libraries(cri_info cri pda)

install(TARGETS cri_info DESTINATION bin)
