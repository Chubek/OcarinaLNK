# Doxygen template preprocessed by CMake configure_file.
PROJECT_NAME           = "OcarinaLNK"
PROJECT_BRIEF          = "Deterministic linker core with stable C ABI"
OUTPUT_DIRECTORY       = @CMAKE_BINARY_DIR@/docs
INPUT                  = @CMAKE_SOURCE_DIR@/include @CMAKE_SOURCE_DIR@/src @CMAKE_SOURCE_DIR@/docs/frontend.md @CMAKE_SOURCE_DIR@/docs/frontned.md @CMAKE_SOURCE_DIR@/docs/manual
FILE_PATTERNS          = *.h *.hpp *.c *.cc *.cpp *.md *.lua
RECURSIVE              = YES
USE_MDFILE_AS_MAINPAGE = @CMAKE_SOURCE_DIR@/docs/frontend.md
GENERATE_HTML          = YES
GENERATE_LATEX         = NO
GENERATE_XML           = YES
EXTRACT_ALL            = YES
EXTRACT_PRIVATE        = NO
EXTRACT_STATIC         = YES
JAVADOC_AUTOBRIEF      = YES
MULTILINE_CPP_IS_BRIEF = YES
QUIET                  = YES
WARN_IF_UNDOCUMENTED   = NO
OPTIMIZE_OUTPUT_FOR_C  = YES
MARKDOWN_SUPPORT       = YES
AUTOLINK_SUPPORT       = YES
TOC_INCLUDE_HEADINGS   = 3
ALIASES               += chapter="\\par Chapter:"
