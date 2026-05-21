cmake_minimum_required(VERSION 3.20)
project(keystone-hello-eapp C CXX ASM)

set(eapp_bin hello)
set(eapp_src hello.c)

# eapp
add_executable(${eapp_bin} ${eapp_src})
target_link_options(${eapp_bin} PRIVATE -static)

install(TARGETS ${eapp_bin})

