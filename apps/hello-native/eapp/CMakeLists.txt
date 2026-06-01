cmake_minimum_required(VERSION 3.20)
project(keystone-hello-native-eapp C CXX ASM)

set(eapp_bin hello)
set(eapp_src hello.c)

# eapp
add_executable(${eapp_bin} ${eapp_src})
target_compile_options(${eapp_bin} PRIVATE -fno-stack-protector)
target_link_options(${eapp_bin} PRIVATE -static -nostdlib)
target_link_libraries(${eapp_bin} keystone-eapp keystone-edge keystone-app-syscall)

install(TARGETS ${eapp_bin})
