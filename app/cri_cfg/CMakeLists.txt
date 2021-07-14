# Copyright 2014, 2016 Dirk Hutter, Jan de Cuveland

add_executable(cri_cfg cri_cfg.cpp)

target_compile_definitions(cri_cfg PUBLIC BOOST_ALL_DYN_LINK)

target_include_directories(cri_cfg SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(cri_cfg
  cri logging
  ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} rt
)

install(TARGETS cri_cfg DESTINATION bin)
