cmake_minimum_required(VERSION 3.15)
project(raft-c C)
set(CMAKE_C_STANDARD 11)

include_directories(include)
file(GLOB SOURCES src/*.c)

add_library(raft ${SOURCES})

add_executable(raftd src/main.c src/raft_fsm.c src/raft_types.c src/raft_network.c)

target_link_libraries(raftd PRIVATE raft pthread)
