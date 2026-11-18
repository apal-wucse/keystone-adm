cmake_minimum_required(VERSION 3.20)
project(keystone-admfail-eapp C CXX ASM)

set(eapp_bin admfail)
set(eapp_src admfail.c)

# eapp
add_executable(${eapp_bin} ${eapp_src})
target_link_options(${eapp_bin} PRIVATE -static)
target_link_libraries(${eapp_bin} PRIVATE keystone-edge keystone-app-syscall)

install(TARGETS ${eapp_bin})
