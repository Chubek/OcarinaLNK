#ifndef OLNK_OLNK_API_H
#define OLNK_OLNK_API_H

/*
 * =============================================================================
 * LLM AGENT NOTES / PATCH GUIDANCE
 * =============================================================================
 *
 * Purpose of this file:
 *   - Define the stable public API boundary for olnk.
 *   - Be safe for external consumers, plugins, tests, and embedding use.
 *   - Offer a C ABI where possible, while remaining pleasant in C++.
 *
 * Current design philosophy:
 *   - Keep opaque handles opaque.
 *   - Expose enough functionality to initialize a linker session, configure it,
 *     run it, and inspect diagnostics/results.
 *   - Avoid overcommitting to internals before src/ implementation settles.
 *
 * Important future improvements:
 *   1. Add richer input specification:
 *        - object files
 *        - archives
 *        - shared libraries
 *        - linker scripts
 *        - in-memory buffers
 *   2. Add explicit target/configuration structs for:
 *        - format (ELF/PE/Mach-O/WASM)
 *        - machine (x86-64/AArch64/etc.)
 *        - endianness / word size / ABI
 *   3. Expand diagnostics:
 *        - source location
 *        - severity
 *        - machine-readable diagnostic codes
 *        - JSON export
 *   4. Add plugin management:
 *        - load/unload by path
 *        - enumerate loaded plugins
 *        - pass plugin-specific configuration blobs
 *   5. Add incremental linking/session caching hooks.
 *   6. Clarify threading guarantees and ownership semantics.
 *   7. Introduce explicit allocator hooks if jemalloc or custom allocators matter.
 *   8. Add symbol/section inspection APIs for tooling.
 *
 * Design constraints:
 *   - This header should remain low-friction to include.
 *   - Keep external ABI conservative and implementation details opaque.
 *   - Avoid exposing STL types or exceptions over the C ABI boundary.
 *
 * If patching:
 *   - Prefer additive evolution via new structs/functions.
 *   - Preserve binary compatibility where feasible.
 *   - If changing structs, append fields and consider size/version fields.
 *   - Document ownership rules for every pointer-returning API.
 *
 * Ownership rules in this header:
 *   - Any `const char*` returned by olnk is owned by olnk unless documented
 *     otherwise, and remains valid until the associated object is destroyed or
 *     until another API call invalidates it.
 *   - Any object returned as a handle must be destroyed by its matching destroy.
 *
 * Error handling policy:
 *   - Functions generally return an `olnk_status_t`.
 *   - Extended error detail is available via diagnostic queries and the session.
 * =============================================================================
 */

#include <stddef.h>
#include <stdint.h>

#include <olnk/olnk-version.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ---------------------------------------------------------------------------
 * Symbol visibility / calling convention
 * ---------------------------------------------------------------------------
 */

#if defined(_WIN32) || defined(__CYGWIN__)
  #if defined(OLNK_BUILD_SHARED)
    #if defined(OLNK_BUILDING_LIBRARY)
      #define OLNK_API __declspec(dllexport)
    #else
      #define OLNK_API __declspec(dllimport)
    #endif
  #else
    #define OLNK_API
  #endif
  #define OLNK_CALL
#else
  #if defined(__GNUC__) || defined(__clang__)
    #define OLNK_API __attribute__((visibility("default")))
  #else
    #define OLNK_API
  #endif
  #define OLNK_CALL
#endif

/*
 * ---------------------------------------------------------------------------
 * Language interop helpers
 * ---------------------------------------------------------------------------
 */

#if defined(__cplusplus)
  #define OLNK_NOEXCEPT noexcept
#else
  #define OLNK_NOEXCEPT
#endif

/*
 * ---------------------------------------------------------------------------
 * Forward declarations for opaque handles
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_context      olnk_context_t;
typedef struct olnk_session      olnk_session_t;
typedef struct olnk_config       olnk_config_t;
typedef struct olnk_result       olnk_result_t;
typedef struct olnk_diagnostic   olnk_diagnostic_t;

/*
 * ---------------------------------------------------------------------------
 * Basic enums
 * ---------------------------------------------------------------------------
 */

typedef enum olnk_status {
    OLNK_STATUS_OK = 0,
    OLNK_STATUS_UNKNOWN = 1,
    OLNK_STATUS_INVALID_ARGUMENT = 2,
    OLNK_STATUS_OUT_OF_MEMORY = 3,
    OLNK_STATUS_IO_ERROR = 4,
    OLNK_STATUS_PARSE_ERROR = 5,
    OLNK_STATUS_FORMAT_ERROR = 6,
    OLNK_STATUS_MACHINE_ERROR = 7,
    OLNK_STATUS_SCRIPT_ERROR = 8,
    OLNK_STATUS_PLUGIN_ERROR = 9,
    OLNK_STATUS_SYMBOL_ERROR = 10,
    OLNK_STATUS_RELOCATION_ERROR = 11,
    OLNK_STATUS_LINK_ERROR = 12,
    OLNK_STATUS_INTERNAL_ERROR = 13,
    OLNK_STATUS_NOT_IMPLEMENTED = 14
} olnk_status_t;

typedef enum olnk_log_level {
    OLNK_LOG_TRACE = 0,
    OLNK_LOG_DEBUG = 1,
    OLNK_LOG_INFO  = 2,
    OLNK_LOG_WARN  = 3,
    OLNK_LOG_ERROR = 4,
    OLNK_LOG_FATAL = 5,
    OLNK_LOG_OFF   = 6
} olnk_log_level_t;

typedef enum olnk_diagnostic_severity {
    OLNK_DIAGNOSTIC_NOTE = 0,
    OLNK_DIAGNOSTIC_WARNING = 1,
    OLNK_DIAGNOSTIC_ERROR = 2,
    OLNK_DIAGNOSTIC_FATAL = 3
} olnk_diagnostic_severity_t;

typedef enum olnk_output_kind {
    OLNK_OUTPUT_EXECUTABLE = 0,
    OLNK_OUTPUT_SHARED_LIBRARY = 1,
    OLNK_OUTPUT_STATIC_IMAGE = 2,
    OLNK_OUTPUT_RELOCATABLE = 3,
    OLNK_OUTPUT_BINARY = 4
} olnk_output_kind_t;

/*
 * ---------------------------------------------------------------------------
 * Callback types
 * ---------------------------------------------------------------------------
 */

typedef void (OLNK_CALL *olnk_log_callback_t)(
    void* user_data,
    olnk_log_level_t level,
    const char* message);

typedef void (OLNK_CALL *olnk_diag_callback_t)(
    void* user_data,
    olnk_diagnostic_severity_t severity,
    const char* message);

/*
 * ---------------------------------------------------------------------------
 * Version / library metadata
 * ---------------------------------------------------------------------------
 */

OLNK_API uint32_t OLNK_CALL olnk_api_version(void) OLNK_NOEXCEPT;
OLNK_API uint32_t OLNK_CALL olnk_version_major(void) OLNK_NOEXCEPT;
OLNK_API uint32_t OLNK_CALL olnk_version_minor(void) OLNK_NOEXCEPT;
OLNK_API uint32_t OLNK_CALL olnk_version_patch(void) OLNK_NOEXCEPT;
OLNK_API const char* OLNK_CALL olnk_version_string(void) OLNK_NOEXCEPT;
OLNK_API const char* OLNK_CALL olnk_status_to_string(olnk_status_t status) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Library lifecycle
 * ---------------------------------------------------------------------------
 *
 * These are optional convenience functions. The implementation may choose to
 * make them no-ops if global initialization is unnecessary.
 */

OLNK_API olnk_status_t OLNK_CALL olnk_initialize(void) OLNK_NOEXCEPT;
OLNK_API void OLNK_CALL olnk_shutdown(void) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Context management
 * ---------------------------------------------------------------------------
 *
 * A context may own process-wide-ish services:
 *   - logging
 *   - allocators
 *   - plugin loader state
 *   - caches
 *   - diagnostics policy
 *
 * A session is typically created from a context.
 */

OLNK_API olnk_context_t* OLNK_CALL olnk_context_create(void);
OLNK_API void OLNK_CALL olnk_context_destroy(olnk_context_t* context) OLNK_NOEXCEPT;

OLNK_API void OLNK_CALL olnk_context_set_log_level(
    olnk_context_t* context,
    olnk_log_level_t level) OLNK_NOEXCEPT;

OLNK_API void OLNK_CALL olnk_context_set_log_callback(
    olnk_context_t* context,
    olnk_log_callback_t callback,
    void* user_data) OLNK_NOEXCEPT;

OLNK_API void OLNK_CALL olnk_context_set_diagnostic_callback(
    olnk_context_t* context,
    olnk_diag_callback_t callback,
    void* user_data) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Config management
 * ---------------------------------------------------------------------------
 *
 * Config is mutable setup state that can be reused across sessions if desired.
 * This keeps the public ABI simple while allowing the implementation to evolve.
 */

OLNK_API olnk_config_t* OLNK_CALL olnk_config_create(void);
OLNK_API void OLNK_CALL olnk_config_destroy(olnk_config_t* config) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_output_path(
    olnk_config_t* config,
    const char* output_path) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_map_path(
    olnk_config_t* config,
    const char* map_path) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_entry_symbol(
    olnk_config_t* config,
    const char* symbol_name) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_output_kind(
    olnk_config_t* config,
    olnk_output_kind_t kind) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_format(
    olnk_config_t* config,
    const char* format_name) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_machine(
    olnk_config_t* config,
    const char* machine_name) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_script_path(
    olnk_config_t* config,
    const char* script_path) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_thread_count(
    olnk_config_t* config,
    uint32_t thread_count) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_incremental(
    olnk_config_t* config,
    int enabled) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_set_debug_info(
    olnk_config_t* config,
    int enabled) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_define(
    olnk_config_t* config,
    const char* key,
    const char* value) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_add_input_file(
    olnk_config_t* config,
    const char* path) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_add_library_path(
    olnk_config_t* config,
    const char* path) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_add_library(
    olnk_config_t* config,
    const char* name) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_config_add_plugin(
    olnk_config_t* config,
    const char* path) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Session management
 * ---------------------------------------------------------------------------
 */

OLNK_API olnk_session_t* OLNK_CALL olnk_session_create(
    olnk_context_t* context,
    const olnk_config_t* config);

OLNK_API void OLNK_CALL olnk_session_destroy(
    olnk_session_t* session) OLNK_NOEXCEPT;

/*
 * Main execution entry point.
 * On success or failure, diagnostics may be queried from the session/result.
 */
OLNK_API olnk_status_t OLNK_CALL olnk_session_run(
    olnk_session_t* session,
    olnk_result_t** out_result) OLNK_NOEXCEPT;

/*
 * Convenience reset API if the implementation supports reusing a session.
 */
OLNK_API olnk_status_t OLNK_CALL olnk_session_reset(
    olnk_session_t* session) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Diagnostic inspection
 * ---------------------------------------------------------------------------
 */

OLNK_API size_t OLNK_CALL olnk_session_diagnostic_count(
    const olnk_session_t* session) OLNK_NOEXCEPT;

OLNK_API const olnk_diagnostic_t* OLNK_CALL olnk_session_diagnostic_at(
    const olnk_session_t* session,
    size_t index) OLNK_NOEXCEPT;

OLNK_API olnk_diagnostic_severity_t OLNK_CALL olnk_diagnostic_severity(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT;

OLNK_API const char* OLNK_CALL olnk_diagnostic_message(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT;

OLNK_API const char* OLNK_CALL olnk_diagnostic_file(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT;

OLNK_API uint32_t OLNK_CALL olnk_diagnostic_line(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT;

OLNK_API uint32_t OLNK_CALL olnk_diagnostic_column(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Result inspection
 * ---------------------------------------------------------------------------
 */

OLNK_API void OLNK_CALL olnk_result_destroy(
    olnk_result_t* result) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_result_status(
    const olnk_result_t* result) OLNK_NOEXCEPT;

OLNK_API const char* OLNK_CALL olnk_result_output_path(
    const olnk_result_t* result) OLNK_NOEXCEPT;

OLNK_API uint64_t OLNK_CALL olnk_result_output_size(
    const olnk_result_t* result) OLNK_NOEXCEPT;

OLNK_API uint64_t OLNK_CALL olnk_result_elapsed_ns(
    const olnk_result_t* result) OLNK_NOEXCEPT;

OLNK_API uint32_t OLNK_CALL olnk_result_input_count(
    const olnk_result_t* result) OLNK_NOEXCEPT;

OLNK_API uint32_t OLNK_CALL olnk_result_warning_count(
    const olnk_result_t* result) OLNK_NOEXCEPT;

OLNK_API uint32_t OLNK_CALL olnk_result_error_count(
    const olnk_result_t* result) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Convenience one-shot API
 * ---------------------------------------------------------------------------
 *
 * Intended for embedding and tests that do not need to manage session objects
 * manually.
 */

OLNK_API olnk_status_t OLNK_CALL olnk_link(
    olnk_context_t* context,
    const olnk_config_t* config,
    olnk_result_t** out_result) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Future-proof extension hook
 * ---------------------------------------------------------------------------
 *
 * Generic escape hatch for experimental features before they become stable API.
 * The implementation may reject unknown keys with NOT_IMPLEMENTED.
 */

OLNK_API olnk_status_t OLNK_CALL olnk_context_set_option(
    olnk_context_t* context,
    const char* key,
    const char* value) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL olnk_session_set_option(
    olnk_session_t* session,
    const char* key,
    const char* value) OLNK_NOEXCEPT;

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OLNK_OLNK_API_H */
