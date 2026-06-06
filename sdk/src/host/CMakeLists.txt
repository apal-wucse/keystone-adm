project(keystone-host CXX)

set(CFLAGS      "-Wall -Werror")
set(LDFLAGS     "-static")

set(SOURCE_FILES
  elf.c
  elf32.c
  elf64.c
  hash_util.cpp
  ElfFile.cpp
  KeystoneDevice.cpp
  Enclave.cpp
  Memory.cpp
  AdditionalData.cpp
  AdditionalDataMemory.cpp
  UniqueFd.cpp
  KeystoneLogs.cpp
  EnclaveBuilder.cpp
)

set(INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include/host)

# Benchmark
option(CS_BENCHMARK "context switch count" OFF)
option(TIME_BENCHMARK "time benchmark" OFF)

if(CS_BENCHMARK)
  add_compile_definitions(CS_BENCHMARK)
endif()

if(TIME_BENCHMARK)
  add_compile_definitions(TIME_BENCHMARK)
endif()

set(CMAKE_C_FLAGS          "${CMAKE_C_FLAGS} ${CFLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${LDFLAGS}")

include_directories(${INCLUDE_DIRS})

add_library(${PROJECT_NAME} STATIC ${SOURCE_FILES} ${COMMON_SOURCE_FILES})

set_target_properties(${PROJECT_NAME} PROPERTIES
  CXX_STANDARD 23
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

install(TARGETS ${PROJECT_NAME} DESTINATION ${out_dir}/lib)
install(DIRECTORY ${INCLUDE_DIRS} DESTINATION ${out_dir}/include)
