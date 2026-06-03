cmake_minimum_required(VERSION 3.20)
project(keystone-hello-host-adm C CXX ASM)

set(host_src host.cpp)
set(host_bin hello-adm-runner)

# host
add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} PRIVATE keystone-host keystone-edge)

install(TARGETS ${host_bin})
