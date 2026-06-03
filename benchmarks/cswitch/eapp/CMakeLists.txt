cmake_minimum_required(VERSION 3.20)
project(keystone-cswitch-eapp C CXX ASM)

set(eapp_bin cswitch)
set(eapp_src eapp.c)

option(ADM_BUILD "Build for ADM" OFF)
option(ADM_RW_BUILD "Build for ADM w/ rw permission" OFF)

if(ADM_BUILD AND ADM_RW_BUILD)
  message(FATAL_ERROR "invalid state")
endif()

# eapp
add_executable(${eapp_bin} ${eapp_src})
target_link_options(${eapp_bin} PRIVATE -static)
target_link_libraries(${eapp_bin} keystone-edge keystone-app-syscall)

if(ADM_BUILD)
  target_compile_definitions(${eapp_bin} PRIVATE ADM_BUILD)
elseif(ADM_RW_BUILD)
  target_compile_definitions(${eapp_bin} PRIVATE ADM_RW_BUILD)
endif()

install(TARGETS ${eapp_bin})
