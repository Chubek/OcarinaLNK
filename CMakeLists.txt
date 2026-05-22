cmake_minimum_required(VERSION 3.20)
project(OcarinaLNK LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(OLNK_BUILD_DOCS "Build documentation" OFF)
option(OLNK_INSTALL_DOCS "Install docs directory" ON)
option(OLNK_INSTALL_FORMATS "Install Lua formats" ON)
option(OLNK_INSTALL_MACHINES "Install Lua machines" ON)
option(OLNK_INSTALL_PLUGINS "Install plugin modules" ON)
option(OLNK_INSTALL_SCRIPTS "Install Lua scripts" ON)
option(OLNK_INSTALL_EXAMPLES "Install examples" ON)

# Default install prefix: ~/.olnk
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.olnk" CACHE PATH "Install path prefix" FORCE)
endif()

# LLM hint:
# Keep install layout centralized and deterministic so subdirectories can reuse
# these cache paths without inventing additional install roots.
set(OLNK_INSTALL_BINDIR "bin" CACHE STRING "Install directory for executables")
set(OLNK_INSTALL_LIBDIR "lib" CACHE STRING "Install directory for libraries")
set(OLNK_INSTALL_LUA_LIBDIR "lib/lua" CACHE STRING "Install directory for Lua libraries")
set(OLNK_INSTALL_FORMATSDIR "formats" CACHE STRING "Install directory for format modules")
set(OLNK_INSTALL_MACHINESDIR "machines" CACHE STRING "Install directory for machine modules")
set(OLNK_INSTALL_PLUGINSDIR "plugins" CACHE STRING "Install directory for plugin modules")
set(OLNK_INSTALL_SCRIPTSDIR "scripts" CACHE STRING "Install directory for linker scripts")
set(OLNK_INSTALL_EXAMPLESDIR "examples" CACHE STRING "Install directory for examples")
set(OLNK_INSTALL_DOCDIR "share/docs" CACHE STRING "Install directory for docs")
set(OLNK_INSTALL_MANDIR "share/man" CACHE STRING "Install directory for man pages")
set(OLNK_INSTALL_PKGCONFIGDIR "${OLNK_INSTALL_LIBDIR}/pkgconfig" CACHE STRING "Install directory for pkg-config metadata")

file(GLOB OLNK_CORE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/src/olnk/*.cpp"
)

add_library(olnk_core STATIC ${OLNK_CORE_SOURCES})
target_include_directories(olnk_core
  PUBLIC
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/third_party/Klyspec"
    "${CMAKE_SOURCE_DIR}/third_party/sol2/include"
    "${CMAKE_SOURCE_DIR}/third_party/lua"
)

file(GLOB OLNK_LUA_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_SOURCE_DIR}/third_party/lua/*.c"
)
list(FILTER OLNK_LUA_SOURCES EXCLUDE REGEX ".*/lua\\.c$")
list(FILTER OLNK_LUA_SOURCES EXCLUDE REGEX ".*/luac\\.c$")
target_sources(olnk_core PRIVATE ${OLNK_LUA_SOURCES})

if(UNIX AND NOT APPLE)
  target_link_libraries(olnk_core PRIVATE dl)
endif()

add_executable(olnk "${CMAKE_SOURCE_DIR}/src/main.cpp")
target_link_libraries(olnk PRIVATE olnk_core)

# Plugin modules
set(OLNK_PLUGIN_SOURCES
  "${CMAKE_SOURCE_DIR}/plugins/append-stub.cpp"
  "${CMAKE_SOURCE_DIR}/plugins/dump-symtbl.cpp"
  "${CMAKE_SOURCE_DIR}/plugins/dwarf-embeddings.cpp"
  "${CMAKE_SOURCE_DIR}/plugins/identical-code-folding.cpp"
  "${CMAKE_SOURCE_DIR}/plugins/linker-map-generation.cpp"
  "${CMAKE_SOURCE_DIR}/plugins/visualization.cpp"
)

foreach(plugin_src IN LISTS OLNK_PLUGIN_SOURCES)
  if(EXISTS "${plugin_src}")
    get_filename_component(plugin_name "${plugin_src}" NAME_WE)
    add_library("${plugin_name}" MODULE "${plugin_src}")
    target_include_directories("${plugin_name}"
      PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${CMAKE_SOURCE_DIR}/third_party/Klyspec"
    )
    set_target_properties("${plugin_name}" PROPERTIES PREFIX "")
  else()
    message(WARNING "Plugin source not found: ${plugin_src}")
  endif()
endforeach()

# Runtime lookup roots
set(OLNK_PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/${OLNK_INSTALL_PLUGINSDIR}" CACHE PATH "Directory containing plugin modules")
set(OLNK_FORMAT_DIR "${CMAKE_INSTALL_PREFIX}/${OLNK_INSTALL_FORMATSDIR}" CACHE PATH "Directory containing Lua format specs")
set(OLNK_MACHINE_DIR "${CMAKE_INSTALL_PREFIX}/${OLNK_INSTALL_MACHINESDIR}" CACHE PATH "Directory containing Lua machine specs")
set(OLNK_EXAMPLES_DIR "${CMAKE_INSTALL_PREFIX}/${OLNK_INSTALL_EXAMPLESDIR}" CACHE PATH "Directory containing examples")

# LLM hint:
# Keep pkg-config generation explicit and install-path aware so downstream
# builds can discover OcarinaLNK deterministically without hardcoded paths.
set(OLNK_PREFIX "${CMAKE_INSTALL_PREFIX}")
configure_file(
  "${CMAKE_SOURCE_DIR}/olnk.pc.in"
  "${CMAKE_CURRENT_BINARY_DIR}/olnk.pc"
  @ONLY
)

if((OLNK_BUILD_DOCS OR OLNK_INSTALL_DOCS) AND EXISTS "${CMAKE_SOURCE_DIR}/docs/CMakeLists.txt")
  add_subdirectory(docs)
endif()

if(OLNK_INSTALL_FORMATS AND EXISTS "${CMAKE_SOURCE_DIR}/formats/CMakeLists.txt")
  add_subdirectory(formats)
endif()

if(OLNK_INSTALL_MACHINES AND EXISTS "${CMAKE_SOURCE_DIR}/machines/CMakeLists.txt")
  add_subdirectory(machines)
endif()

if(OLNK_INSTALL_PLUGINS AND EXISTS "${CMAKE_SOURCE_DIR}/plugins/CMakeLists.txt")
  add_subdirectory(plugins)
endif()

if(OLNK_INSTALL_SCRIPTS AND EXISTS "${CMAKE_SOURCE_DIR}/scripts/CMakeLists.txt")
  add_subdirectory(scripts)
endif()

# Install executable
install(TARGETS olnk
  RUNTIME DESTINATION "${OLNK_INSTALL_BINDIR}"
)

if(OLNK_INSTALL_EXAMPLES)
  install(DIRECTORY "${CMAKE_SOURCE_DIR}/examples/"
    DESTINATION "${OLNK_INSTALL_EXAMPLESDIR}"
    OPTIONAL
  )
endif()

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/olnk.pc"
  DESTINATION "${OLNK_INSTALL_PKGCONFIGDIR}"
)

if(EXISTS "${CMAKE_SOURCE_DIR}/man/man1")
  install(FILES
    "${CMAKE_SOURCE_DIR}/man/man1/olnk.1"
    "${CMAKE_SOURCE_DIR}/man/man1/olnkidl.1"
    "${CMAKE_SOURCE_DIR}/man/man1/olnkquery.1"
    DESTINATION "${OLNK_INSTALL_MANDIR}/man1"
    OPTIONAL
  )
endif()

if(EXISTS "${CMAKE_SOURCE_DIR}/man/man3")
  install(FILES
    "${CMAKE_SOURCE_DIR}/man/man3/olnk-api.3"
    "${CMAKE_SOURCE_DIR}/man/man3/olnk-format.3"
    "${CMAKE_SOURCE_DIR}/man/man3/olnk-ir.3"
    "${CMAKE_SOURCE_DIR}/man/man3/olnk-machine.3"
    "${CMAKE_SOURCE_DIR}/man/man3/olnk-plugin.3"
    "${CMAKE_SOURCE_DIR}/man/man3/olnk-version.3"
    DESTINATION "${OLNK_INSTALL_MANDIR}/man3"
    OPTIONAL
  )
endif()
