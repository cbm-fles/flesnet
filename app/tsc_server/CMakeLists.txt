# Copyright 2025 Dirk Hutter

add_executable(tsc_server tsc_server.cpp ComponentBuilder.cpp cri_source.cpp SubTimesliceItemProducer.cpp)

target_include_directories(tsc_server SYSTEM PUBLIC ${Boost_INCLUDE_DIRS})

target_link_libraries(tsc_server pda cri fles_ipc fles_core logging ${Boost_LIBRARIES})

install(TARGETS tsc_server DESTINATION bin)
