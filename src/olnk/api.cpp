#include <olnk/olnk-api.h>
#include <olnk/olnk-format.h>
#include <olnk/olnk-machine.h>

#include <atomic>
#include <cstring>
#include <fstream>
#include <new>
#include <string>
#include <utility>
#include <vector>

/*
 * =============================================================================
 * LLM AGENT NOTES / IMPLEMENTATION GUIDANCE
 * =============================================================================
 *
 * This file implements the stable public API declared in `olnk-api.h`.
 *
 * Current implementation strategy:
 *   - Provide a conservative, robust baseline implementation of the public API.
 *   - Keep object ownership explicit and predictable.
 *   - Avoid exceptions escaping over the public ABI boundary.
 *   - Be useful for tests, embedding, and incremental development even before a
 *     real linker backend is fully wired in.
 *
 * Important limitation:
 *   - This file does NOT implement a full in-process linker pipeline yet.
 *   - `olnk_session_run()` calls the internal pipeline and returns
 *     `NOT_IMPLEMENTED` until output emission internals are fully wired.
 *
 * Why this is still valuable:
 *   - External users can already exercise lifecycle and memory management.
 *   - Tests can validate configuration and diagnostics flows.
 *   - Future backend work can plug into these opaque objects without breaking
 *     the public ABI.
 *
 * Future patching guidance:
 *   1. Keep all opaque structs private to this translation unit or private
 *      internal headers.
 *   2. Preserve public API behavior unless intentionally evolving it.
 *   3. If a real backend is introduced, replace the internals of
 *      `olnk_session_run()` first, rather than redesigning object lifetimes.
 *   4. Avoid storing raw pointers to caller-owned strings; copy into owned
 *      storage instead.
 *   5. Keep callbacks best-effort and non-throwing.
 * =============================================================================
 */

namespace {

/*
 * =============================================================================
 * Internal helper types
 * =============================================================================
 *
 * LLM AGENT NOTES:
 *   The public header exposes opaque handles. We define them privately here as
 *   C++ structs using STL internally. This is acceptable because STL types do
 *   not cross the ABI boundary.
 */

struct DiagnosticStorage {
    olnk_diagnostic_severity_t severity = OLNK_DIAGNOSTIC_NOTE;
    std::string message;
    std::string file;
    uint32_t line = 0;
    uint32_t column = 0;
};

struct ResultStorage {
    olnk_status_t status = OLNK_STATUS_UNKNOWN;
    std::string output_path;
    uint64_t output_size = 0;
    uint64_t elapsed_ns = 0;
    uint32_t input_count = 0;
    uint32_t warning_count = 0;
    uint32_t error_count = 0;
};

} // namespace

/*
 * =============================================================================
 * Opaque handle definitions
 * =============================================================================
 */

struct olnk_context {
    olnk_log_level_t log_level = OLNK_LOG_INFO;
    olnk_log_callback_t log_callback = nullptr;
    void* log_user_data = nullptr;
    olnk_diag_callback_t diag_callback = nullptr;
    void* diag_user_data = nullptr;

    /*
     * LLM AGENT NOTES:
     *   This option bag is the implementation of the generic extension hook
     *   `olnk_context_set_option()`.
     *
     *   Long-term, if context options become stable API, consider lifting common
     *   keys into dedicated typed fields in the public header.
     */
    std::vector<std::pair<std::string, std::string>> options;
};

struct olnk_config {
    std::string output_path;
    std::string map_path;
    std::string entry_symbol;
    olnk_output_kind_t output_kind = OLNK_OUTPUT_EXECUTABLE;
    std::string format_name;
    std::string machine_name;
    std::string script_path;
    uint32_t thread_count = 0;
    int incremental = 0;
    int debug_info = 0;

    std::vector<std::pair<std::string, std::string>> defines;
    std::vector<std::string> input_files;
    std::vector<std::string> library_paths;
    std::vector<std::string> libraries;
    std::vector<std::string> plugins;
};

struct olnk_diagnostic {
    DiagnosticStorage data;
};

struct olnk_result {
    ResultStorage data;
};

struct olnk_session {
    olnk_context_t* context = nullptr;

    /*
     * LLM AGENT NOTES:
     *   We copy config state into the session rather than retaining a borrowed
     *   pointer. This avoids subtle lifetime issues if the caller destroys or
     *   mutates the config after session creation.
     */
    olnk_config snapshot_config;

    std::vector<olnk_diagnostic_t*> diagnostics;

    /*
     * Generic session option storage for `olnk_session_set_option()`.
     */
    std::vector<std::pair<std::string, std::string>> options;
};

/*
 * =============================================================================
 * Internal utilities
 * =============================================================================
 */

static std::atomic<uint32_t> g_initialize_count {0};
namespace olnk {
olnk_status_t run_link_pipeline();
olnk_status_t run_format_serializer(const olnk_format_definition_t* definition,
                                    olnk_format_context_t* context,
                                    const olnk_format_image_view_t* image_view,
                                    olnk_format_output_info_t* out_info);
}

static const char* k_unknown_status_string = "unknown";
static const char* k_invalid_status_string = "invalid-argument";
static const char* k_oom_status_string = "out-of-memory";
static const char* k_io_status_string = "io-error";
static const char* k_parse_status_string = "parse-error";
static const char* k_format_status_string = "format-error";
static const char* k_machine_status_string = "machine-error";
static const char* k_script_status_string = "script-error";
static const char* k_plugin_status_string = "plugin-error";
static const char* k_symbol_status_string = "symbol-error";
static const char* k_relocation_status_string = "relocation-error";
static const char* k_link_status_string = "link-error";
static const char* k_internal_status_string = "internal-error";
static const char* k_not_implemented_status_string = "not-implemented";
static const char* k_ok_status_string = "ok";

static bool valid_log_level(olnk_log_level_t level) noexcept
{
    switch (level) {
        case OLNK_LOG_TRACE:
        case OLNK_LOG_DEBUG:
        case OLNK_LOG_INFO:
        case OLNK_LOG_WARN:
        case OLNK_LOG_ERROR:
        case OLNK_LOG_FATAL:
        case OLNK_LOG_OFF:
            return true;
        default:
            return false;
    }
}

static bool valid_diag_severity(olnk_diagnostic_severity_t severity) noexcept
{
    switch (severity) {
        case OLNK_DIAGNOSTIC_NOTE:
        case OLNK_DIAGNOSTIC_WARNING:
        case OLNK_DIAGNOSTIC_ERROR:
        case OLNK_DIAGNOSTIC_FATAL:
            return true;
        default:
            return false;
    }
}

static bool valid_output_kind(olnk_output_kind_t kind) noexcept
{
    switch (kind) {
        case OLNK_OUTPUT_EXECUTABLE:
        case OLNK_OUTPUT_SHARED_LIBRARY:
        case OLNK_OUTPUT_STATIC_IMAGE:
        case OLNK_OUTPUT_RELOCATABLE:
        case OLNK_OUTPUT_BINARY:
            return true;
        default:
            return false;
    }
}

static void safe_emit_log(
    olnk_context_t* context,
    olnk_log_level_t level,
    const char* message) noexcept
{
    if (context == nullptr || message == nullptr) {
        return;
    }

    if (!valid_log_level(level)) {
        return;
    }

    /*
     * LLM AGENT NOTES:
     *   We interpret larger numeric values as more severe, except `OFF`, which
     *   disables callback emission entirely.
     */
    if (context->log_level == OLNK_LOG_OFF) {
        return;
    }

    if (level < context->log_level) {
        return;
    }

    if (context->log_callback != nullptr) {
        context->log_callback(context->log_user_data, level, message);
    }
}

static olnk_status_t add_diagnostic(
    olnk_session_t* session,
    olnk_diagnostic_severity_t severity,
    const char* message,
    const char* file,
    uint32_t line,
    uint32_t column) noexcept
{
    if (session == nullptr || message == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (!valid_diag_severity(severity)) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_diagnostic_t* diagnostic = nullptr;
    try {
        diagnostic = new olnk_diagnostic_t();
        diagnostic->data.severity = severity;
        diagnostic->data.message = message;
        if (file != nullptr) {
            diagnostic->data.file = file;
        }
        diagnostic->data.line = line;
        diagnostic->data.column = column;
        session->diagnostics.push_back(diagnostic);
    } catch (const std::bad_alloc&) {
        delete diagnostic;
        return OLNK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        delete diagnostic;
        return OLNK_STATUS_INTERNAL_ERROR;
    }

    if (session->context != nullptr && session->context->diag_callback != nullptr) {
        session->context->diag_callback(
            session->context->diag_user_data,
            severity,
            diagnostic->data.message.c_str());
    }

    return OLNK_STATUS_OK;
}

static void clear_diagnostics(olnk_session_t* session) noexcept
{
    if (session == nullptr) {
        return;
    }

    for (olnk_diagnostic_t* d : session->diagnostics) {
        delete d;
    }
    session->diagnostics.clear();
}

static uint32_t count_diagnostics_with_severity(
    const olnk_session_t* session,
    olnk_diagnostic_severity_t severity) noexcept
{
    if (session == nullptr) {
        return 0;
    }

    uint32_t count = 0;
    for (const olnk_diagnostic_t* d : session->diagnostics) {
        if (d != nullptr && d->data.severity == severity) {
            ++count;
        }
    }
    return count;
}

static olnk_status_t set_string(std::string& out, const char* value) noexcept
{
    if (value == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    try {
        out = value;
        return OLNK_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return OLNK_STATUS_INTERNAL_ERROR;
    }
}

template <typename Vec>
static olnk_status_t push_back_string(Vec& vec, const char* value) noexcept
{
    if (value == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    try {
        vec.emplace_back(value);
        return OLNK_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return OLNK_STATUS_INTERNAL_ERROR;
    }
}

static olnk_status_t define_kv(
    std::vector<std::pair<std::string, std::string>>& vec,
    const char* key,
    const char* value) noexcept
{
    if (key == nullptr || value == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    try {
        vec.emplace_back(std::string(key), std::string(value));
        return OLNK_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return OLNK_STATUS_INTERNAL_ERROR;
    }
}

static olnk_result_t* make_result() noexcept
{
    try {
        return new olnk_result_t();
    } catch (...) {
        return nullptr;
    }
}

static void populate_result_from_session(
    olnk_result_t* result,
    const olnk_session_t* session,
    olnk_status_t status,
    uint64_t elapsed_ns) noexcept
{
    if (result == nullptr) {
        return;
    }

    result->data.status = status;
    result->data.elapsed_ns = elapsed_ns;

    if (session != nullptr) {
        result->data.input_count =
            static_cast<uint32_t>(session->snapshot_config.input_files.size());
        result->data.warning_count =
            count_diagnostics_with_severity(session, OLNK_DIAGNOSTIC_WARNING);
        result->data.error_count =
            count_diagnostics_with_severity(session, OLNK_DIAGNOSTIC_ERROR) +
            count_diagnostics_with_severity(session, OLNK_DIAGNOSTIC_FATAL);

        result->data.output_path = session->snapshot_config.output_path;
    }
}

static uint64_t file_size_or_zero(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        return 0;
    }
    const std::streamoff size = in.tellg();
    if (size < 0) {
        return 0;
    }
    return static_cast<uint64_t>(size);
}

static olnk_status_t write_output_file(const std::string& path,
                                       const void* bytes,
                                       size_t size) noexcept
{
    if (path.empty() || bytes == nullptr || size == 0u) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return OLNK_STATUS_IO_ERROR;
    }

    out.write(static_cast<const char*>(bytes), static_cast<std::streamsize>(size));
    if (!out.good()) {
        return OLNK_STATUS_IO_ERROR;
    }

    return OLNK_STATUS_OK;
}

static olnk_status_t OLNK_CALL api_format_emit_output(olnk_format_context_t* format_ctx,
                                                      const void* bytes,
                                                      size_t size) OLNK_NOEXCEPT
{
    if (format_ctx == nullptr || format_ctx->session == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    return write_output_file(format_ctx->session->snapshot_config.output_path, bytes, size);
}

static olnk_status_t OLNK_CALL api_format_emit_aux_file(olnk_format_context_t*,
                                                        const char*,
                                                        const void*,
                                                        size_t) OLNK_NOEXCEPT
{
    return OLNK_STATUS_NOT_IMPLEMENTED;
}

static void OLNK_CALL api_format_log(olnk_format_context_t* format_ctx,
                                     olnk_log_level_t level,
                                     const char* message) OLNK_NOEXCEPT
{
    if (format_ctx == nullptr) {
        return;
    }
    safe_emit_log(format_ctx->linker_context, level, message);
}

static void OLNK_CALL api_format_diagnostic(olnk_format_context_t* format_ctx,
                                            olnk_diagnostic_severity_t severity,
                                            const char* message) OLNK_NOEXCEPT
{
    if (format_ctx == nullptr || format_ctx->session == nullptr) {
        return;
    }
    (void)add_diagnostic(format_ctx->session, severity, message ? message : "", nullptr, 0, 0);
}

static const char* OLNK_CALL api_format_get_option(olnk_format_context_t*,
                                                   const char*) OLNK_NOEXCEPT
{
    return nullptr;
}

static olnk_status_t OLNK_CALL api_format_set_option(olnk_format_context_t*,
                                                     const char*,
                                                     const char*) OLNK_NOEXCEPT
{
    return OLNK_STATUS_NOT_IMPLEMENTED;
}

/*
 * =============================================================================
 * Version / metadata API
 * =============================================================================
 */

extern "C" OLNK_API uint32_t OLNK_CALL olnk_api_version(void) OLNK_NOEXCEPT
{
    /*
     * LLM AGENT NOTES:
     *   The public API header includes `olnk-version.h`, which should define the
     *   library's version macros. If those macros are absent in a future patch,
     *   update this function rather than hardcoding scattered literals.
     */
#ifdef OLNK_VERSION_ENCODED
    return OLNK_VERSION_ENCODED;
#else
    /*
     * Conservative fallback for partially wired builds.
     */
    return 0u;
#endif
}

extern "C" OLNK_API uint32_t OLNK_CALL olnk_version_major(void) OLNK_NOEXCEPT
{
#ifdef OLNK_VERSION_MAJOR
    return OLNK_VERSION_MAJOR;
#else
    return 0u;
#endif
}

extern "C" OLNK_API uint32_t OLNK_CALL olnk_version_minor(void) OLNK_NOEXCEPT
{
#ifdef OLNK_VERSION_MINOR
    return OLNK_VERSION_MINOR;
#else
    return 0u;
#endif
}

extern "C" OLNK_API uint32_t OLNK_CALL olnk_version_patch(void) OLNK_NOEXCEPT
{
#ifdef OLNK_VERSION_PATCH
    return OLNK_VERSION_PATCH;
#else
    return 0u;
#endif
}

extern "C" OLNK_API const char* OLNK_CALL olnk_version_string(void) OLNK_NOEXCEPT
{
#ifdef OLNK_VERSION_STRING
    return OLNK_VERSION_STRING;
#else
    return "0.0.0";
#endif
}

extern "C" OLNK_API const char* OLNK_CALL
olnk_status_to_string(olnk_status_t status) OLNK_NOEXCEPT
{
    switch (status) {
        case OLNK_STATUS_OK: return k_ok_status_string;
        case OLNK_STATUS_UNKNOWN: return k_unknown_status_string;
        case OLNK_STATUS_INVALID_ARGUMENT: return k_invalid_status_string;
        case OLNK_STATUS_OUT_OF_MEMORY: return k_oom_status_string;
        case OLNK_STATUS_IO_ERROR: return k_io_status_string;
        case OLNK_STATUS_PARSE_ERROR: return k_parse_status_string;
        case OLNK_STATUS_FORMAT_ERROR: return k_format_status_string;
        case OLNK_STATUS_MACHINE_ERROR: return k_machine_status_string;
        case OLNK_STATUS_SCRIPT_ERROR: return k_script_status_string;
        case OLNK_STATUS_PLUGIN_ERROR: return k_plugin_status_string;
        case OLNK_STATUS_SYMBOL_ERROR: return k_symbol_status_string;
        case OLNK_STATUS_RELOCATION_ERROR: return k_relocation_status_string;
        case OLNK_STATUS_LINK_ERROR: return k_link_status_string;
        case OLNK_STATUS_INTERNAL_ERROR: return k_internal_status_string;
        case OLNK_STATUS_NOT_IMPLEMENTED: return k_not_implemented_status_string;
        default: return "unrecognized-status";
    }
}

/*
 * =============================================================================
 * Library lifecycle
 * =============================================================================
 */

extern "C" OLNK_API olnk_status_t OLNK_CALL olnk_initialize(void) OLNK_NOEXCEPT
{
    /*
     * LLM AGENT NOTES:
     *   Global initialization is intentionally lightweight and idempotent.
     *   A reference count allows callers to pair initialize/shutdown without
     *   forcing singleton semantics on embedders.
     */
    g_initialize_count.fetch_add(1, std::memory_order_relaxed);
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API void OLNK_CALL olnk_shutdown(void) OLNK_NOEXCEPT
{
    uint32_t current = g_initialize_count.load(std::memory_order_relaxed);
    while (current != 0u &&
           !g_initialize_count.compare_exchange_weak(
               current,
               current - 1u,
               std::memory_order_relaxed,
               std::memory_order_relaxed)) {
    }

    /*
     * LLM AGENT NOTES:
     *   No global resources exist yet, so shutdown is a no-op after balancing
     *   the init counter. If process-global caches or plugin registries are
     *   added later, clean them up when the count reaches zero.
     */
}

/*
 * =============================================================================
 * Context management
 * =============================================================================
 */

extern "C" OLNK_API olnk_context_t* OLNK_CALL olnk_context_create(void)
{
    try {
        return new olnk_context_t();
    } catch (...) {
        return nullptr;
    }
}

extern "C" OLNK_API void OLNK_CALL
olnk_context_destroy(olnk_context_t* context) OLNK_NOEXCEPT
{
    delete context;
}

extern "C" OLNK_API void OLNK_CALL
olnk_context_set_log_level(
    olnk_context_t* context,
    olnk_log_level_t level) OLNK_NOEXCEPT
{
    if (context == nullptr) {
        return;
    }

    if (!valid_log_level(level)) {
        return;
    }

    context->log_level = level;
}

extern "C" OLNK_API void OLNK_CALL
olnk_context_set_log_callback(
    olnk_context_t* context,
    olnk_log_callback_t callback,
    void* user_data) OLNK_NOEXCEPT
{
    if (context == nullptr) {
        return;
    }

    context->log_callback = callback;
    context->log_user_data = user_data;
}

extern "C" OLNK_API void OLNK_CALL
olnk_context_set_diagnostic_callback(
    olnk_context_t* context,
    olnk_diag_callback_t callback,
    void* user_data) OLNK_NOEXCEPT
{
    if (context == nullptr) {
        return;
    }

    context->diag_callback = callback;
    context->diag_user_data = user_data;
}

/*
 * =============================================================================
 * Config management
 * =============================================================================
 */

extern "C" OLNK_API olnk_config_t* OLNK_CALL olnk_config_create(void)
{
    try {
        return new olnk_config_t();
    } catch (...) {
        return nullptr;
    }
}

extern "C" OLNK_API void OLNK_CALL
olnk_config_destroy(olnk_config_t* config) OLNK_NOEXCEPT
{
    delete config;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_output_path(
    olnk_config_t* config,
    const char* output_path) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return set_string(config->output_path, output_path);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_map_path(
    olnk_config_t* config,
    const char* map_path) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return set_string(config->map_path, map_path);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_entry_symbol(
    olnk_config_t* config,
    const char* symbol_name) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return set_string(config->entry_symbol, symbol_name);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_output_kind(
    olnk_config_t* config,
    olnk_output_kind_t kind) OLNK_NOEXCEPT
{
    if (config == nullptr || !valid_output_kind(kind)) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    config->output_kind = kind;
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_format(
    olnk_config_t* config,
    const char* format_name) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return set_string(config->format_name, format_name);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_machine(
    olnk_config_t* config,
    const char* machine_name) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return set_string(config->machine_name, machine_name);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_script_path(
    olnk_config_t* config,
    const char* script_path) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return set_string(config->script_path, script_path);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_thread_count(
    olnk_config_t* config,
    uint32_t thread_count) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    config->thread_count = thread_count;
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_incremental(
    olnk_config_t* config,
    int enabled) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    config->incremental = (enabled != 0) ? 1 : 0;
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_set_debug_info(
    olnk_config_t* config,
    int enabled) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    config->debug_info = (enabled != 0) ? 1 : 0;
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_define(
    olnk_config_t* config,
    const char* key,
    const char* value) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return define_kv(config->defines, key, value);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_add_input_file(
    olnk_config_t* config,
    const char* path) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return push_back_string(config->input_files, path);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_add_library_path(
    olnk_config_t* config,
    const char* path) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return push_back_string(config->library_paths, path);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_add_library(
    olnk_config_t* config,
    const char* name) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return push_back_string(config->libraries, name);
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_config_add_plugin(
    olnk_config_t* config,
    const char* path) OLNK_NOEXCEPT
{
    if (config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return push_back_string(config->plugins, path);
}

/*
 * =============================================================================
 * Session management
 * =============================================================================
 */

extern "C" OLNK_API olnk_session_t* OLNK_CALL
olnk_session_create(
    olnk_context_t* context,
    const olnk_config_t* config)
{
    if (context == nullptr || config == nullptr) {
        return nullptr;
    }

    try {
        olnk_session_t* session = new olnk_session_t();
        session->context = context;
        session->snapshot_config = *config;

        safe_emit_log(context, OLNK_LOG_DEBUG, "created olnk session");
        return session;
    } catch (...) {
        return nullptr;
    }
}

extern "C" OLNK_API void OLNK_CALL
olnk_session_destroy(olnk_session_t* session) OLNK_NOEXCEPT
{
    if (session == nullptr) {
        return;
    }

    clear_diagnostics(session);
    delete session;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_session_run(
    olnk_session_t* session,
    olnk_result_t** out_result) OLNK_NOEXCEPT
{
    if (session == nullptr || out_result == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    *out_result = nullptr;
    clear_diagnostics(session);

    safe_emit_log(session->context, OLNK_LOG_INFO, "starting olnk session");

    olnk_result_t* result = make_result();
    if (result == nullptr) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    }

    // LLM hint:
    // `olnk` must remain self-hosted. This entrypoint deliberately avoids
    // shelling out to external toolchains and invokes only internal pipeline
    // and format serializer ABI hooks.
    olnk_status_t final_status = OLNK_STATUS_OK;
    if (session->snapshot_config.input_files.empty()) {
        const olnk_status_t diag_status = add_diagnostic(
            session, OLNK_DIAGNOSTIC_ERROR, "no input files configured", nullptr, 0, 0);
        if (diag_status != OLNK_STATUS_OK) {
            delete result;
            return diag_status;
        }
        final_status = OLNK_STATUS_INVALID_ARGUMENT;
    } else if (session->snapshot_config.output_path.empty()) {
        const olnk_status_t diag_status = add_diagnostic(
            session, OLNK_DIAGNOSTIC_ERROR, "no output path configured", nullptr, 0, 0);
        if (diag_status != OLNK_STATUS_OK) {
            delete result;
            return diag_status;
        }
        final_status = OLNK_STATUS_INVALID_ARGUMENT;
    } else {
        try {
            final_status = olnk::run_link_pipeline();
            if (final_status == OLNK_STATUS_OK) {
                const char* format_name = session->snapshot_config.format_name.empty()
                                              ? "ELF"
                                              : session->snapshot_config.format_name.c_str();
                const olnk_format_definition_t* format_def = olnk_find_format(format_name);
                if (format_def == nullptr) {
                    const std::string msg = std::string("unknown format: ") + format_name;
                    const olnk_status_t diag_status = add_diagnostic(
                        session, OLNK_DIAGNOSTIC_ERROR, msg.c_str(), nullptr, 0, 0);
                    if (diag_status != OLNK_STATUS_OK) {
                        delete result;
                        return diag_status;
                    }
                    final_status = OLNK_STATUS_INVALID_ARGUMENT;
                }
                const char* machine_name = session->snapshot_config.machine_name.empty()
                                               ? "x86_64"
                                               : session->snapshot_config.machine_name.c_str();
                const olnk_machine_definition_t* machine_def = olnk_find_machine(machine_name);
                if (final_status == OLNK_STATUS_OK && machine_def == nullptr) {
                    const std::string msg = std::string("unknown machine: ") + machine_name;
                    const olnk_status_t diag_status = add_diagnostic(
                        session, OLNK_DIAGNOSTIC_ERROR, msg.c_str(), nullptr, 0, 0);
                    if (diag_status != OLNK_STATUS_OK) {
                        delete result;
                        return diag_status;
                    }
                    final_status = OLNK_STATUS_INVALID_ARGUMENT;
                }

                if (final_status == OLNK_STATUS_OK) {
                    const olnk_format_host_t host = {
                        OLNK_FORMAT_ABI_VERSION,
                        static_cast<uint32_t>(sizeof(olnk_format_host_t)),
                        api_format_log,
                        api_format_diagnostic,
                        api_format_get_option,
                        api_format_set_option,
                        api_format_emit_output,
                        api_format_emit_aux_file,
                        nullptr,
                        nullptr,
                        nullptr
                    };
                    olnk_format_context_t format_ctx {};
                    format_ctx.host = &host;
                    format_ctx.linker_context = session->context;
                    format_ctx.session = session;
                    format_ctx.config = &session->snapshot_config;
                    format_ctx.reserved_0 =
                        const_cast<olnk_machine_definition_t*>(machine_def);

                    olnk_format_image_view_t image_view {};
                    image_view.opaque = nullptr;
                    image_view.opaque_size = 0u;

                    olnk_format_output_info_t out_info {};
                    final_status = olnk::run_format_serializer(
                        format_def, &format_ctx, &image_view, &out_info);
                    if (final_status == OLNK_STATUS_OK) {
                        result->data.output_size = out_info.file_size;
                    }
                }
            }
            if (final_status == OLNK_STATUS_NOT_IMPLEMENTED) {
                const olnk_status_t diag_status = add_diagnostic(
                    session,
                    OLNK_DIAGNOSTIC_ERROR,
                    "internal linker feature is not fully wired yet",
                    nullptr,
                    0,
                    0);
                if (diag_status != OLNK_STATUS_OK) {
                    delete result;
                    return diag_status;
                }
            }
        } catch (const std::bad_alloc&) {
            delete result;
            return OLNK_STATUS_OUT_OF_MEMORY;
        } catch (...) {
            delete result;
            return OLNK_STATUS_INTERNAL_ERROR;
        }
    }

    const uint64_t elapsed_ns = 0u;
    populate_result_from_session(result, session, final_status, elapsed_ns);
    if (final_status != OLNK_STATUS_OK) {
        result->data.output_size = 0u;
    } else if (result->data.output_size == 0u) {
        result->data.output_size = file_size_or_zero(session->snapshot_config.output_path);
    }

    *out_result = result;

    if (final_status == OLNK_STATUS_OK) {
        safe_emit_log(session->context, OLNK_LOG_INFO, "olnk session completed");
    } else {
        safe_emit_log(session->context, OLNK_LOG_ERROR, "olnk session failed: invalid configuration");
    }
    return final_status;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_session_reset(
    olnk_session_t* session) OLNK_NOEXCEPT
{
    if (session == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    clear_diagnostics(session);
    session->options.clear();

    safe_emit_log(session->context, OLNK_LOG_DEBUG, "reset olnk session");
    return OLNK_STATUS_OK;
}

/*
 * =============================================================================
 * Diagnostic inspection
 * =============================================================================
 */

extern "C" OLNK_API size_t OLNK_CALL
olnk_session_diagnostic_count(
    const olnk_session_t* session) OLNK_NOEXCEPT
{
    if (session == nullptr) {
        return 0u;
    }
    return session->diagnostics.size();
}

extern "C" OLNK_API const olnk_diagnostic_t* OLNK_CALL
olnk_session_diagnostic_at(
    const olnk_session_t* session,
    size_t index) OLNK_NOEXCEPT
{
    if (session == nullptr || index >= session->diagnostics.size()) {
        return nullptr;
    }
    return session->diagnostics[index];
}

extern "C" OLNK_API olnk_diagnostic_severity_t OLNK_CALL
olnk_diagnostic_severity(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT
{
    if (diagnostic == nullptr) {
        return OLNK_DIAGNOSTIC_FATAL;
    }
    return diagnostic->data.severity;
}

extern "C" OLNK_API const char* OLNK_CALL
olnk_diagnostic_message(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT
{
    if (diagnostic == nullptr) {
        return nullptr;
    }
    return diagnostic->data.message.c_str();
}

extern "C" OLNK_API const char* OLNK_CALL
olnk_diagnostic_file(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT
{
    if (diagnostic == nullptr) {
        return nullptr;
    }

    if (diagnostic->data.file.empty()) {
        return nullptr;
    }

    return diagnostic->data.file.c_str();
}

extern "C" OLNK_API uint32_t OLNK_CALL
olnk_diagnostic_line(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT
{
    if (diagnostic == nullptr) {
        return 0u;
    }
    return diagnostic->data.line;
}

extern "C" OLNK_API uint32_t OLNK_CALL
olnk_diagnostic_column(
    const olnk_diagnostic_t* diagnostic) OLNK_NOEXCEPT
{
    if (diagnostic == nullptr) {
        return 0u;
    }
    return diagnostic->data.column;
}

/*
 * =============================================================================
 * Result inspection
 * =============================================================================
 */

extern "C" OLNK_API void OLNK_CALL
olnk_result_destroy(
    olnk_result_t* result) OLNK_NOEXCEPT
{
    delete result;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_result_status(
    const olnk_result_t* result) OLNK_NOEXCEPT
{
    if (result == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return result->data.status;
}

extern "C" OLNK_API const char* OLNK_CALL
olnk_result_output_path(
    const olnk_result_t* result) OLNK_NOEXCEPT
{
    if (result == nullptr) {
        return nullptr;
    }

    if (result->data.output_path.empty()) {
        return nullptr;
    }

    return result->data.output_path.c_str();
}

extern "C" OLNK_API uint64_t OLNK_CALL
olnk_result_output_size(
    const olnk_result_t* result) OLNK_NOEXCEPT
{
    if (result == nullptr) {
        return 0u;
    }
    return result->data.output_size;
}

extern "C" OLNK_API uint64_t OLNK_CALL
olnk_result_elapsed_ns(
    const olnk_result_t* result) OLNK_NOEXCEPT
{
    if (result == nullptr) {
        return 0u;
    }
    return result->data.elapsed_ns;
}

extern "C" OLNK_API uint32_t OLNK_CALL
olnk_result_input_count(
    const olnk_result_t* result) OLNK_NOEXCEPT
{
    if (result == nullptr) {
        return 0u;
    }
    return result->data.input_count;
}

extern "C" OLNK_API uint32_t OLNK_CALL
olnk_result_warning_count(
    const olnk_result_t* result) OLNK_NOEXCEPT
{
    if (result == nullptr) {
        return 0u;
    }
    return result->data.warning_count;
}

extern "C" OLNK_API uint32_t OLNK_CALL
olnk_result_error_count(
    const olnk_result_t* result) OLNK_NOEXCEPT
{
    if (result == nullptr) {
        return 0u;
    }
    return result->data.error_count;
}

/*
 * =============================================================================
 * One-shot convenience API
 * =============================================================================
 */

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_link(
    olnk_context_t* context,
    const olnk_config_t* config,
    olnk_result_t** out_result) OLNK_NOEXCEPT
{
    if (context == nullptr || config == nullptr || out_result == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    *out_result = nullptr;

    olnk_session_t* session = olnk_session_create(context, config);
    if (session == nullptr) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    }

    olnk_status_t status = olnk_session_run(session, out_result);
    olnk_session_destroy(session);
    return status;
}

/*
 * =============================================================================
 * Extension option hooks
 * =============================================================================
 */

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_context_set_option(
    olnk_context_t* context,
    const char* key,
    const char* value) OLNK_NOEXCEPT
{
    if (context == nullptr || key == nullptr || value == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    try {
        context->options.emplace_back(std::string(key), std::string(value));
        return OLNK_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return OLNK_STATUS_INTERNAL_ERROR;
    }
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_session_set_option(
    olnk_session_t* session,
    const char* key,
    const char* value) OLNK_NOEXCEPT
{
    if (session == nullptr || key == nullptr || value == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    try {
        session->options.emplace_back(std::string(key), std::string(value));
        return OLNK_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return OLNK_STATUS_INTERNAL_ERROR;
    }
}
