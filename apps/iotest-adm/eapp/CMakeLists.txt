cmake_minimum_required(VERSION 3.20)
project(keystone-iotest-adm-eapp C CXX ASM)

set(eapp_bin iotest-adm)
set(eapp_src iotest.c)

# eapp
add_executable(${eapp_bin} ${eapp_src})
target_link_options(${eapp_bin} PRIVATE -static)

install(TARGETS ${eapp_bin})
