# LLM / maintainer hints:
# - Plugin targets are declared in root CMakeLists for stable ordering.
# - This file handles installation only to avoid duplicate target declarations.

if(NOT OLNK_INSTALL_PLUGINS)
  return()
endif()

set(OLNK_PLUGIN_TARGETS
  append-stub
  dump-symtbl
  dwarf-embeddings
  identical-code-folding
  linker-map-generation
  visualization
)

foreach(plugin_target IN LISTS OLNK_PLUGIN_TARGETS)
  if(TARGET "${plugin_target}")
    install(TARGETS "${plugin_target}"
      LIBRARY DESTINATION "${OLNK_INSTALL_PLUGINSDIR}"
      RUNTIME DESTINATION "${OLNK_INSTALL_PLUGINSDIR}"
    )
  endif()
endforeach()
