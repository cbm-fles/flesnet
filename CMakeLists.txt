cmake_minimum_required (VERSION 2.6) 
project (RDMA)

set (CMAKE_C_FLAGS "-std=c99 -D_POSIX_SOURCE -Wall -pedantic")

add_executable (client client.c) 
add_executable (server server.c) 

target_link_libraries (client rdmacm) 
target_link_libraries (server rdmacm) 

