cmake_minimum_required(VERSION 3.20)
project(keystone-admtest-host C CXX ASM)

set(host_bin admtest-runner)
set(host_src host.cpp)

# host
add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} PRIVATE keystone-host keystone-edge)

install(TARGETS ${host_bin})
