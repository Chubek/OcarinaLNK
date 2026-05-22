# LLM / maintainer hints:
# - Install Lua format definitions only; do not invent build targets here.
# - Keep install paths rooted in top-level OLNK_INSTALL_* variables.

if(NOT OLNK_INSTALL_FORMATS)
  return()
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
  DESTINATION "${OLNK_INSTALL_FORMATSDIR}"
  FILES_MATCHING
    PATTERN "*.lua"
)
