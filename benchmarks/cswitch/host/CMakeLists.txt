cmake_minimum_required(VERSION 3.20)
project(keystone-cswitch-host C CXX ASM)

set(host_src host.cpp)
set(host_bin cswitch-runner)

option(ADM_BUILD "Build for ADM" OFF)

# host
add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} PRIVATE keystone-host keystone-edge)

if(ADM_BUILD)
    target_compile_definitions(${host_bin} PRIVATE ADM_BUILD)
endif()

install(TARGETS ${host_bin})
