cmake_minimum_required(VERSION 3.20)
project(keystone-iozone-host CXX)

set(ecall_src ecall.cpp)
set(ecall_bin iozone-ecall-runner)

set(adm_src adm.cpp)
set(adm_bin iozone-adm-runner)

add_executable(${ecall_bin} ${ecall_src})
target_link_options(${ecall_bin} PRIVATE -static)
target_link_libraries(${ecall_bin} PRIVATE keystone-host keystone-edge)
target_compile_features(${ecall_bin} PRIVATE cxx_std_23)

add_executable(${adm_bin} ${adm_src})
target_link_options(${adm_bin} PRIVATE -static)
target_link_libraries(${adm_bin} PRIVATE keystone-host keystone-edge)
target_compile_features(${adm_bin} PRIVATE cxx_std_23)

install(TARGETS ${ecall_bin} ${adm_bin})
