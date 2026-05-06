set(LOADER_SOURCES loader.S loader-binary.c)
set(LOADER_LINK_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/loader.lds)

add_executable(loader ${LOADER_SOURCES})
target_link_libraries(loader rt_call ld_mm rt_util rt_loader)
target_link_options(loader PRIVATE -static -nostdlib -T ${LOADER_LINK_SCRIPT})

set(OUTPUT_BIN ${CMAKE_CURRENT_BINARY_DIR}/loader.bin)
add_custom_command(
  OUTPUT ${OUTPUT_BIN}
  COMMAND ${CMAKE_OBJCOPY} -O binary --only-section .text
          $<TARGET_FILE:loader>
          ${OUTPUT_BIN}
  DEPENDS loader
  VERBATIM
)

add_custom_target(eyrie-ld ALL DEPENDS ${OUTPUT_BIN})

install(FILES $<TARGET_FILE:loader> ${OUTPUT_BIN} DESTINATION ${CMAKE_INSTALL_BINDIR})
