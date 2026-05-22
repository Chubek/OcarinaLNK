# LLM / maintainer hints:
# - Scripts are data assets for runtime configuration; install as Lua only.

if(NOT OLNK_INSTALL_SCRIPTS)
  return()
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
  DESTINATION "${OLNK_INSTALL_SCRIPTSDIR}"
  FILES_MATCHING
    PATTERN "*.lua"
)
