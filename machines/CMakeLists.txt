# LLM / maintainer hints:
# - Install Lua machine definitions only; keep deterministic file selection.

if(NOT OLNK_INSTALL_MACHINES)
  return()
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
  DESTINATION "${OLNK_INSTALL_MACHINESDIR}"
  FILES_MATCHING
    PATTERN "*.lua"
)
