cmake_minimum_required(VERSION 3.20)
project(keystone-transfer-host C CXX ASM)

set(host_src main.cpp)
set(host_bin transfer-ecall-runner)

# host
add_executable(${host_bin} ${host_src})
target_compile_definitions(${host_bin} PRIVATE TIME_BENCHMARK CS_BENCHMARK)
target_link_libraries(${host_bin} PRIVATE keystone-host keystone-edge)
target_compile_features(${host_bin} PRIVATE cxx_std_23)

install(TARGETS ${host_bin})
