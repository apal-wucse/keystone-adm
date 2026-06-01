cmake_minimum_required(VERSION 3.20)
project(keystone-hello-host C CXX ASM)

set(host_src host.cpp)
set(host_bin hello-runner)

# host
add_executable(${host_bin} ${host_src})
target_link_libraries(${host_bin} PRIVATE keystone-host keystone-edge)
set_target_properties(${host_bin}
  PROPERTIES CXX_STANDARD 11 CXX_STANDARD_REQUIRED YES CXX_EXTENSIONS NO
)

install(TARGETS ${host_bin})

