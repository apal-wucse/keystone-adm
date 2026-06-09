cmake_minimum_required(VERSION 3.20)
project(keystone-transfer-eapp C CXX ASM)

set(eapp_src main.c)
set(eapp_bin transfer-ecall)

# eapp
add_executable(${eapp_bin} ${eapp_src})
target_compile_definitions(${eapp_bin} PRIVATE TIME_BENCHMARK CS_BENCHMARK)
target_link_options(${eapp_bin} PRIVATE -static)
target_link_libraries(${eapp_bin} PRIVATE keystone-edge keystone-app-syscall keystone-eapp)

install(TARGETS ${eapp_bin})
