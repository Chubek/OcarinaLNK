# LLM / maintainer hints:
# - Keep Doxygen wiring isolated in docs/ so root CMakeLists stays focused on build targets.
# - Frontpage is docs/frontend.md and manual chapters are embedded via docs/manual input.

find_package(Doxygen)

if(OLNK_BUILD_DOCS AND DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxygen.i
                 ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)

  add_custom_target(docs
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating Doxygen manual-infused API docs"
    VERBATIM)
endif()

if(OLNK_INSTALL_DOCS)
  install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/"
    DESTINATION "${OLNK_INSTALL_DOCDIR}"
    FILES_MATCHING
      PATTERN "*.md"
      PATTERN "*.i"
  )
endif()
