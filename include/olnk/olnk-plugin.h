#ifndef OLNK_OLNK_PLUGIN_H
#define OLNK_OLNK_PLUGIN_H

/*
 * =============================================================================
 * LLM AGENT NOTES / PATCH GUIDANCE
 * =============================================================================
 *
 * Purpose of this file:
 *   - Define the public plugin ABI for olnk.
 *   - Allow external shared-library plugins to register functionality with the
 *     linker without exposing linker internals directly.
 *   - Keep the ABI conservative and C-compatible, even though the host is
 *     likely implemented in C++.
 *
 * Current design:
 *   - A plugin exposes a single entrypoint:
 *         olnk_plugin_query()
 *     which returns a description table and callback set.
 *   - The host provides a small service API through `olnk_plugin_host_t`.
 *   - The plugin receives an execution context (`olnk_plugin_context_t`) during
 *     lifecycle callbacks.
 *
 * Major future improvements:
 *   1. Add event hooks for finer-grained linker phases:
 *        - before input scan
 *        - after input parse
 *        - before/after symbol resolution
 *        - before/after relocation
 *        - before output serialization
 *   2. Add typed configuration schema support rather than stringly-typed
 *      key/value options only.
 *   3. Add plugin capability negotiation:
 *        - required host features
 *        - optional host features
 *        - minimum host ABI
 *   4. Add richer data model accessors for:
 *        - IR traversal
 *        - symbols/sections
 *        - relocation graph
 *        - memory layout
 *   5. Clarify thread-safety:
 *        - whether callbacks may run concurrently
 *        - whether a plugin instance is shared across sessions
 *   6. Add structured error reporting with source ranges and codes.
 *   7. Add explicit unload policy if plugins can keep external resources alive.
 *
 * Constraints:
 *   - Avoid STL types and exceptions across the ABI boundary.
 *   - Avoid embedding implementation-owned structs in the ABI.
 *   - Keep pointers opaque where internals are not stable yet.
 *   - Preserve backward compatibility by appending fields and adding new
 *     callback tables instead of mutating old signatures drastically.
 *
 * Ownership rules:
 *   - Strings returned by the plugin through descriptor fields must remain valid
 *     for the lifetime of the loaded plugin module, unless otherwise stated.
 *   - Opaque handles provided by the host are owned by the host.
 *   - Plugin-private state created by the plugin is owned by the plugin and
 *     destroyed through plugin callbacks.
 *
 * If patching:
 *   - Prefer additive changes.
 *   - If ABI needs to evolve, bump OLNK_PLUGIN_ABI_VERSION and keep a
 *     compatibility path if feasible.
 *   - Consider adding struct size/version fields before introducing optional
 *     trailing fields.
 * =============================================================================
 */

#include <stddef.h>
#include <stdint.h>

#include <olnk/olnk-api.h>
#include <olnk/olnk-version.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ---------------------------------------------------------------------------
 * Plugin ABI version
 * ---------------------------------------------------------------------------
 *
 * Bump this when the binary contract between host and plugin changes in a way
 * that is not backward-compatible.
 */
#define OLNK_PLUGIN_ABI_VERSION 1u

/*
 * ---------------------------------------------------------------------------
 * Forward declarations / opaque handles
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_plugin_context olnk_plugin_context_t;
typedef struct olnk_plugin_instance olnk_plugin_instance_t;

/*
 * ---------------------------------------------------------------------------
 * Plugin kinds / capabilities / event phases
 * ---------------------------------------------------------------------------
 */

typedef enum olnk_plugin_kind {
    OLNK_PLUGIN_KIND_UNKNOWN = 0,
    OLNK_PLUGIN_KIND_ANALYSIS = 1,
    OLNK_PLUGIN_KIND_TRANSFORM = 2,
    OLNK_PLUGIN_KIND_OUTPUT = 3,
    OLNK_PLUGIN_KIND_DIAGNOSTIC = 4,
    OLNK_PLUGIN_KIND_SCRIPTING = 5
} olnk_plugin_kind_t;

typedef enum olnk_plugin_event {
    OLNK_PLUGIN_EVENT_NONE = 0,
    OLNK_PLUGIN_EVENT_INITIALIZE = 1,
    OLNK_PLUGIN_EVENT_BEFORE_LINK = 2,
    OLNK_PLUGIN_EVENT_AFTER_LINK = 3,
    OLNK_PLUGIN_EVENT_FINALIZE = 4
} olnk_plugin_event_t;

/*
 * Bitmask capability flags.
 * These are advisory for now and can be expanded over time.
 */
typedef enum olnk_plugin_capability {
    OLNK_PLUGIN_CAPABILITY_NONE                = 0u,
    OLNK_PLUGIN_CAPABILITY_READ_DIAGNOSTICS    = 1u << 0,
    OLNK_PLUGIN_CAPABILITY_EMIT_DIAGNOSTICS    = 1u << 1,
    OLNK_PLUGIN_CAPABILITY_READ_CONFIG         = 1u << 2,
    OLNK_PLUGIN_CAPABILITY_WRITE_CONFIG        = 1u << 3,
    OLNK_PLUGIN_CAPABILITY_READ_IR             = 1u << 4,
    OLNK_PLUGIN_CAPABILITY_WRITE_IR            = 1u << 5,
    OLNK_PLUGIN_CAPABILITY_READ_LAYOUT         = 1u << 6,
    OLNK_PLUGIN_CAPABILITY_WRITE_OUTPUT_AUX    = 1u << 7
} olnk_plugin_capability_t;

/*
 * ---------------------------------------------------------------------------
 * Host service API exposed to plugins
 * ---------------------------------------------------------------------------
 *
 * Notes:
 *   - The host table is intentionally small at first.
 *   - Plugins should always null-check optional function pointers.
 *   - Future versions may append fields.
 */

typedef struct olnk_plugin_host {
    uint32_t abi_version;
    uint32_t struct_size;

    /*
     * Logging and diagnostics.
     */
    void (OLNK_CALL *log)(
        olnk_plugin_context_t* plugin_ctx,
        olnk_log_level_t level,
        const char* message);

    void (OLNK_CALL *diagnostic)(
        olnk_plugin_context_t* plugin_ctx,
        olnk_diagnostic_severity_t severity,
        const char* message);

    /*
     * Key/value configuration access.
     *
     * `get_option` returns NULL if the key does not exist.
     * Returned pointer is host-owned and valid at least for the current
     * callback invocation unless implementation documents stronger guarantees.
     */
    const char* (OLNK_CALL *get_option)(
        olnk_plugin_context_t* plugin_ctx,
        const char* key);

    olnk_status_t (OLNK_CALL *set_option)(
        olnk_plugin_context_t* plugin_ctx,
        const char* key,
        const char* value);

    /*
     * Auxiliary artifact emission.
     * Useful for map files, reports, visualizations, debug dumps, etc.
     */
    olnk_status_t (OLNK_CALL *emit_file)(
        olnk_plugin_context_t* plugin_ctx,
        const char* logical_name,
        const void* data,
        size_t size);

    /*
     * Reserved for ABI growth. Must be zero/null initialized by host.
     */
    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
} olnk_plugin_host_t;

/*
 * ---------------------------------------------------------------------------
 * Plugin execution context
 * ---------------------------------------------------------------------------
 *
 * Opaque session-scoped context passed into plugin callbacks.
 * The plugin may use it only through host service functions or future accessor
 * APIs.
 */

struct olnk_plugin_context {
    const olnk_plugin_host_t* host;
    olnk_context_t* linker_context;
    olnk_session_t* session;
    const olnk_config_t* config;
    void* reserved_0;
    void* reserved_1;
};

/*
 * ---------------------------------------------------------------------------
 * Plugin descriptor
 * ---------------------------------------------------------------------------
 *
 * Static metadata describing a plugin.
 * Recommended that these strings live in static storage.
 */

typedef struct olnk_plugin_descriptor {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;
    const char* description;
    const char* author;
    const char* license;

    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;

    olnk_plugin_kind_t kind;
    uint32_t capabilities; /* bitmask of olnk_plugin_capability_t */

    /*
     * Optional compatibility hints.
     * These refer to host/plugin ABI compatibility, not necessarily semantic
     * package versioning.
     */
    uint32_t min_host_api_version;
    uint32_t max_host_api_version;

    /*
     * Reserved for future expansion.
     */
    const char* homepage;
    const char* repository;
    void* reserved_0;
    void* reserved_1;
} olnk_plugin_descriptor_t;

/*
 * ---------------------------------------------------------------------------
 * Plugin callback table
 * ---------------------------------------------------------------------------
 *
 * Lifecycle:
 *   1. Host loads plugin shared library.
 *   2. Host calls `olnk_plugin_query()` to obtain this table.
 *   3. For each use, host may create an instance via `create`.
 *   4. Host may configure the instance with `set_option`.
 *   5. Host invokes lifecycle hooks.
 *   6. Host destroys the instance via `destroy`.
 *
 * Threading:
 *   - For now, assume callbacks are invoked serially per instance unless
 *     documented otherwise by the host.
 *   - Do not assume cross-session serialization.
 */

typedef struct olnk_plugin_vtable {
    uint32_t abi_version;
    uint32_t struct_size;

    /*
     * Creates plugin-private instance state.
     *
     * `out_instance` must be set by the plugin on success.
     */
    olnk_status_t (OLNK_CALL *create)(
        const olnk_plugin_descriptor_t* descriptor,
        olnk_plugin_instance_t** out_instance);

    /*
     * Destroys plugin-private instance state created by `create`.
     * Safe to call with NULL if plugin chooses to support that.
     */
    void (OLNK_CALL *destroy)(
        olnk_plugin_instance_t* instance);

    /*
     * Optional configuration hook.
     * Called zero or more times before initialize/before_link.
     */
    olnk_status_t (OLNK_CALL *set_option)(
        olnk_plugin_instance_t* instance,
        const char* key,
        const char* value);

    /*
     * Optional lifecycle hooks.
     */
    olnk_status_t (OLNK_CALL *initialize)(
        olnk_plugin_instance_t* instance,
        olnk_plugin_context_t* context);

    olnk_status_t (OLNK_CALL *before_link)(
        olnk_plugin_instance_t* instance,
        olnk_plugin_context_t* context);

    olnk_status_t (OLNK_CALL *after_link)(
        olnk_plugin_instance_t* instance,
        olnk_plugin_context_t* context,
        const olnk_result_t* result);

    olnk_status_t (OLNK_CALL *finalize)(
        olnk_plugin_instance_t* instance,
        olnk_plugin_context_t* context);

    /*
     * Optional generic event hook for future extensibility.
     * Can be NULL if plugin only implements dedicated hooks above.
     */
    olnk_status_t (OLNK_CALL *on_event)(
        olnk_plugin_instance_t* instance,
        olnk_plugin_context_t* context,
        olnk_plugin_event_t event,
        const void* event_data,
        size_t event_size);

    /*
     * Reserved for ABI growth.
     */
    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
} olnk_plugin_vtable_t;

/*
 * ---------------------------------------------------------------------------
 * Plugin query object
 * ---------------------------------------------------------------------------
 *
 * Returned by the plugin entrypoint so the host can inspect metadata and
 * callback tables in one stable object.
 */

typedef struct olnk_plugin_query_result {
    uint32_t abi_version;
    uint32_t struct_size;

    const olnk_plugin_descriptor_t* descriptor;
    const olnk_plugin_vtable_t* vtable;

    /*
     * Optional plugin-defined opaque pointer for static metadata or module-level
     * services. Host must treat this as read-only and opaque.
     */
    const void* module_data;

    void* reserved_0;
    void* reserved_1;
} olnk_plugin_query_result_t;

/*
 * ---------------------------------------------------------------------------
 * Entrypoint name and function signature
 * ---------------------------------------------------------------------------
 *
 * Shared libraries should export a function with this exact signature:
 *
 *   OLNK_API const olnk_plugin_query_result_t* OLNK_CALL
 *   olnk_plugin_query(void);
 *
 * The returned pointer should remain valid for the lifetime of the module.
 */

typedef const olnk_plugin_query_result_t* (OLNK_CALL *olnk_plugin_query_fn_t)(void);

#define OLNK_PLUGIN_ENTRYPOINT_NAME "olnk_plugin_query"

/*
 * Helper macro for plugin authors.
 */
#define OLNK_DECLARE_PLUGIN_ENTRYPOINT() \
    OLNK_API const olnk_plugin_query_result_t* OLNK_CALL olnk_plugin_query(void)

/*
 * ---------------------------------------------------------------------------
 * Optional convenience helpers for plugin implementations
 * ---------------------------------------------------------------------------
 */

static inline uint32_t
olnk_plugin_make_version(uint32_t major, uint32_t minor, uint32_t patch)
{
    return ((major & 0x3FFu) << 22) |
           ((minor & 0x3FFu) << 12) |
           ((patch & 0xFFFu) << 0);
}

static inline int
olnk_plugin_host_supports(
    const olnk_plugin_host_t* host,
    uint32_t required_abi_version)
{
    return host != NULL && host->abi_version >= required_abi_version;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OLNK_OLNK_PLUGIN_H */
