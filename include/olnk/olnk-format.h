#ifndef OLNK_OLNK_FORMAT_H
#define OLNK_OLNK_FORMAT_H

/*
 * =============================================================================
 * LLM AGENT NOTES / PATCH GUIDANCE
 * =============================================================================
 *
 * Purpose of this file:
 *   - Define the public ABI for output/object "formats" in olnk.
 *   - A format is something like ELF, PE, Mach-O, WASM object/container, etc.
 *   - Formats describe file-level behavior, object semantics, section naming
 *     conventions, headers, symbol semantics, relocation namespace hints, and
 *     serialization hooks.
 *
 * Intended usage:
 *   - Built-in formats may be implemented internally in C++ and adapted to this
 *     ABI.
 *   - External formats may eventually be provided by plugins or Lua-backed
 *     modules and surfaced through a stable descriptor/vtable contract.
 *   - The linker core should be able to query a format by name and invoke its
 *     callbacks without knowing implementation details.
 *
 * Current design goals:
 *   - Keep the ABI C-compatible and conservative.
 *   - Avoid leaking STL/C++ types.
 *   - Model enough capability to register/identify a format and let the linker
 *     delegate validation, defaults, and final serialization behavior.
 *
 * Major future improvements:
 *   1. Add richer object-file parsing hooks if formats also parse input objects.
 *   2. Add explicit relocation-kind translation tables and relocation writers.
 *   3. Add symbol binding/visibility normalization APIs.
 *   4. Add import/export table, dynamic linking, TLS, unwind, and debug format
 *      hooks.
 *   5. Add detailed section flag translation and segment/partition planning.
 *   6. Add fine-grained header population callbacks:
 *        - file header
 *        - program headers / load commands / section table
 *        - data directories
 *   7. Add versioned config schemas and validation diagnostics.
 *   8. Split object-input format vs final-image/output format if the design
 *      later needs that separation.
 *
 * Constraints:
 *   - This header should stay lightweight.
 *   - Opaque handles should remain opaque.
 *   - Any ABI growth should prefer struct size/version fields and appended
 *     members.
 *
 * Ownership rules:
 *   - Descriptor strings should generally be static/module-lifetime strings.
 *   - Host-owned pointers passed into callbacks must not be freed by formats.
 *   - Returned pointers from lookup APIs are host-owned unless documented
 *     otherwise.
 *
 * If patching:
 *   - Prefer additive changes.
 *   - Do not expose unstable linker internals casually.
 *   - If the binary contract changes incompatibly, bump
 *     OLNK_FORMAT_ABI_VERSION.
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
 * Format ABI version
 * ---------------------------------------------------------------------------
 */

#define OLNK_FORMAT_ABI_VERSION 1u

/*
 * ---------------------------------------------------------------------------
 * Forward declarations / opaque handles
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_format               olnk_format_t;
typedef struct olnk_format_instance      olnk_format_instance_t;
typedef struct olnk_format_context       olnk_format_context_t;
typedef struct olnk_format_image_view    olnk_format_image_view_t;

/*
 * ---------------------------------------------------------------------------
 * Format classes / capabilities / flags
 * ---------------------------------------------------------------------------
 */

typedef enum olnk_format_kind {
    OLNK_FORMAT_KIND_UNKNOWN = 0,
    OLNK_FORMAT_KIND_ELF = 1,
    OLNK_FORMAT_KIND_PE = 2,
    OLNK_FORMAT_KIND_MACH_O = 3,
    OLNK_FORMAT_KIND_WASM = 4,
    OLNK_FORMAT_KIND_RAW_BINARY = 5,
    OLNK_FORMAT_KIND_CUSTOM = 255
} olnk_format_kind_t;

/*
 * Advisory capabilities. These are feature hints used by the core to determine
 * whether a format supports specific workflows.
 */
typedef enum olnk_format_capability {
    OLNK_FORMAT_CAPABILITY_NONE                    = 0u,
    OLNK_FORMAT_CAPABILITY_EXECUTABLE             = 1u << 0,
    OLNK_FORMAT_CAPABILITY_SHARED_LIBRARY         = 1u << 1,
    OLNK_FORMAT_CAPABILITY_RELOCATABLE_OUTPUT     = 1u << 2,
    OLNK_FORMAT_CAPABILITY_STATIC_IMAGE           = 1u << 3,
    OLNK_FORMAT_CAPABILITY_DEBUG_INFO             = 1u << 4,
    OLNK_FORMAT_CAPABILITY_TLS                    = 1u << 5,
    OLNK_FORMAT_CAPABILITY_DYNAMIC_LINKING        = 1u << 6,
    OLNK_FORMAT_CAPABILITY_IMPORT_EXPORT_TABLES   = 1u << 7,
    OLNK_FORMAT_CAPABILITY_CUSTOM_SECTIONS        = 1u << 8,
    OLNK_FORMAT_CAPABILITY_POSITION_INDEPENDENT   = 1u << 9,
    OLNK_FORMAT_CAPABILITY_INCREMENTAL_LINK       = 1u << 10
} olnk_format_capability_t;

/*
 * Section/segment behavior flags returned or accepted by format helpers.
 * These are intentionally generic and are expected to be translated into
 * format-native flags by implementations.
 */
typedef enum olnk_format_section_flags {
    OLNK_FORMAT_SECTION_FLAG_NONE          = 0u,
    OLNK_FORMAT_SECTION_FLAG_ALLOC         = 1u << 0,
    OLNK_FORMAT_SECTION_FLAG_LOAD          = 1u << 1,
    OLNK_FORMAT_SECTION_FLAG_READ          = 1u << 2,
    OLNK_FORMAT_SECTION_FLAG_WRITE         = 1u << 3,
    OLNK_FORMAT_SECTION_FLAG_EXECUTE       = 1u << 4,
    OLNK_FORMAT_SECTION_FLAG_CODE          = 1u << 5,
    OLNK_FORMAT_SECTION_FLAG_DATA          = 1u << 6,
    OLNK_FORMAT_SECTION_FLAG_BSS           = 1u << 7,
    OLNK_FORMAT_SECTION_FLAG_TLS           = 1u << 8,
    OLNK_FORMAT_SECTION_FLAG_DEBUG         = 1u << 9,
    OLNK_FORMAT_SECTION_FLAG_DISCARDABLE   = 1u << 10,
    OLNK_FORMAT_SECTION_FLAG_MERGE         = 1u << 11,
    OLNK_FORMAT_SECTION_FLAG_STRINGS       = 1u << 12
} olnk_format_section_flags_t;

/*
 * Generic format image class.
 */
typedef enum olnk_format_image_class {
    OLNK_FORMAT_IMAGE_CLASS_UNKNOWN = 0,
    OLNK_FORMAT_IMAGE_CLASS_32 = 1,
    OLNK_FORMAT_IMAGE_CLASS_64 = 2
} olnk_format_image_class_t;

/*
 * Generic endianness.
 */
typedef enum olnk_format_endianness {
    OLNK_FORMAT_ENDIANNESS_UNKNOWN = 0,
    OLNK_FORMAT_ENDIANNESS_LITTLE = 1,
    OLNK_FORMAT_ENDIANNESS_BIG = 2
} olnk_format_endianness_t;

/*
 * ---------------------------------------------------------------------------
 * Generic format properties / output summary
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_format_properties {
    uint32_t abi_version;
    uint32_t struct_size;

    olnk_format_kind_t kind;
    uint32_t capabilities; /* bitmask of olnk_format_capability_t */

    olnk_format_image_class_t image_class;
    olnk_format_endianness_t endianness;

    /*
     * Alignment preferences/hints. Zero means "implementation default".
     */
    uint64_t default_file_alignment;
    uint64_t default_section_alignment;
    uint64_t default_page_alignment;

    /*
     * Naming conventions. Strings are optional and may be NULL.
     */
    const char* default_text_section_name;
    const char* default_data_section_name;
    const char* default_bss_section_name;
    const char* default_rodata_section_name;

    void* reserved_0;
    void* reserved_1;
} olnk_format_properties_t;

/*
 * A lightweight generic description of the produced image. This is intentionally
 * generic; format-specific details should remain internal until the API matures.
 */
typedef struct olnk_format_output_info {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* output_path;
    uint64_t file_size;

    uint64_t image_base;
    uint64_t entry_address;

    uint32_t section_count;
    uint32_t segment_count;

    olnk_output_kind_t output_kind;
    olnk_format_kind_t format_kind;

    void* reserved_0;
    void* reserved_1;
} olnk_format_output_info_t;

/*
 * ---------------------------------------------------------------------------
 * Host service API exposed to format implementations
 * ---------------------------------------------------------------------------
 *
 * The host table gives format code controlled access to logging, config, and
 * output emission. Future versions may expose IR/section/symbol builders here.
 */

typedef struct olnk_format_host {
    uint32_t abi_version;
    uint32_t struct_size;

    void (OLNK_CALL *log)(
        olnk_format_context_t* format_ctx,
        olnk_log_level_t level,
        const char* message);

    void (OLNK_CALL *diagnostic)(
        olnk_format_context_t* format_ctx,
        olnk_diagnostic_severity_t severity,
        const char* message);

    const char* (OLNK_CALL *get_option)(
        olnk_format_context_t* format_ctx,
        const char* key);

    olnk_status_t (OLNK_CALL *set_option)(
        olnk_format_context_t* format_ctx,
        const char* key,
        const char* value);

    /*
     * Output emission helper. A format implementation may either serialize
     * directly using this helper or, in future revisions, use lower-level
     * writer APIs.
     */
    olnk_status_t (OLNK_CALL *emit_output)(
        olnk_format_context_t* format_ctx,
        const void* data,
        size_t size);

    /*
     * Emit auxiliary files such as maps, manifests, metadata, or sidecar debug
     * information.
     */
    olnk_status_t (OLNK_CALL *emit_aux_file)(
        olnk_format_context_t* format_ctx,
        const char* logical_name,
        const void* data,
        size_t size);

    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
} olnk_format_host_t;

/*
 * ---------------------------------------------------------------------------
 * Format execution context
 * ---------------------------------------------------------------------------
 *
 * Session-scoped, host-owned context passed to format callbacks.
 */

struct olnk_format_context {
    const olnk_format_host_t* host;
    olnk_context_t* linker_context;
    olnk_session_t* session;
    const olnk_config_t* config;

    /*
     * Reserved for future access to IR/layout/output builders.
     */
    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
};

/*
 * ---------------------------------------------------------------------------
 * Format descriptor
 * ---------------------------------------------------------------------------
 *
 * Static metadata describing a format implementation.
 */

typedef struct olnk_format_descriptor {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;         /* e.g. "ELF", "PE", "Mach-O" */
    const char* description;
    const char* vendor;
    const char* license;

    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;

    olnk_format_kind_t kind;
    uint32_t capabilities; /* bitmask of olnk_format_capability_t */

    /*
     * Compatibility with the host API surface.
     */
    uint32_t min_host_api_version;
    uint32_t max_host_api_version;

    /*
     * Common aliases:
     *   "elf", "ELF64", "pe", "coff", "mach-o", etc.
     * Optional; may be NULL.
     */
    const char* primary_alias;
    const char* secondary_alias;

    void* reserved_0;
    void* reserved_1;
} olnk_format_descriptor_t;

/*
 * ---------------------------------------------------------------------------
 * Optional input image view
 * ---------------------------------------------------------------------------
 *
 * A generic, opaque view over already-prepared linker image state that a format
 * can use during final validation/serialization. This remains intentionally
 * opaque for now because the core IR is still expected to evolve.
 */

struct olnk_format_image_view {
    const void* opaque;
    size_t opaque_size;
};

/*
 * ---------------------------------------------------------------------------
 * Format callback table
 * ---------------------------------------------------------------------------
 *
 * Lifecycle model:
 *   1. Host resolves a format implementation by name.
 *   2. Host may create a per-session instance.
 *   3. Host configures the format with set_option().
 *   4. Host may ask it to validate config and provide defaults/properties.
 *   5. Host invokes serialize() to produce the final file.
 *   6. Host destroys the instance.
 *
 * All callbacks are optional unless otherwise noted, but practical
 * implementations should generally provide at least:
 *   - create
 *   - destroy
 *   - get_properties
 *   - validate_config
 *   - serialize
 */

typedef struct olnk_format_vtable {
    uint32_t abi_version;
    uint32_t struct_size;

    olnk_status_t (OLNK_CALL *create)(
        const olnk_format_descriptor_t* descriptor,
        olnk_format_instance_t** out_instance);

    void (OLNK_CALL *destroy)(
        olnk_format_instance_t* instance);

    /*
     * Optional per-instance configuration.
     */
    olnk_status_t (OLNK_CALL *set_option)(
        olnk_format_instance_t* instance,
        const char* key,
        const char* value);

    /*
     * Query generic format properties.
     * `out_properties` must be filled by the callee.
     */
    olnk_status_t (OLNK_CALL *get_properties)(
        olnk_format_instance_t* instance,
        olnk_format_properties_t* out_properties);

    /*
     * Validate that the current config/output kind/settings are supported by
     * this format. Emit diagnostics through the host as needed.
     */
    olnk_status_t (OLNK_CALL *validate_config)(
        olnk_format_instance_t* instance,
        olnk_format_context_t* context);

    /*
     * Optional callback to provide format-specific defaults into the host via
     * set_option().
     */
    olnk_status_t (OLNK_CALL *apply_defaults)(
        olnk_format_instance_t* instance,
        olnk_format_context_t* context);

    /*
     * Optional canonicalization hook for section names/flags.
     * `out_name` is host-owned output storage supplied by the caller.
     *
     * Returns:
     *   - OK if canonicalization succeeded
     *   - NOT_IMPLEMENTED if unsupported
     *
     * If implemented, the format should write a NUL-terminated string into
     * out_name when capacity > 0.
     */
    olnk_status_t (OLNK_CALL *canonicalize_section_name)(
        olnk_format_instance_t* instance,
        const char* input_name,
        uint32_t section_flags,
        char* out_name,
        size_t out_name_capacity);

    /*
     * Final serialization step.
     * The host passes an opaque image/layout view prepared by the linker core.
     * The format writes the final bytes through host callbacks.
     */
    olnk_status_t (OLNK_CALL *serialize)(
        olnk_format_instance_t* instance,
        olnk_format_context_t* context,
        const olnk_format_image_view_t* image_view,
        olnk_format_output_info_t* out_info);

    /*
     * Optional summary/string for diagnostics or tooling.
     * Returned pointer is owned by the format/module and should remain valid
     * at least for the instance lifetime.
     */
    const char* (OLNK_CALL *describe)(
        const olnk_format_instance_t* instance);

    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
} olnk_format_vtable_t;

/*
 * ---------------------------------------------------------------------------
 * Format definition object
 * ---------------------------------------------------------------------------
 *
 * A host can bundle descriptor + vtable into this single object for registry
 * purposes.
 */

typedef struct olnk_format_definition {
    uint32_t abi_version;
    uint32_t struct_size;

    const olnk_format_descriptor_t* descriptor;
    const olnk_format_vtable_t* vtable;

    const void* module_data;

    void* reserved_0;
    void* reserved_1;
} olnk_format_definition_t;

/*
 * ---------------------------------------------------------------------------
 * Public lookup / registry-facing APIs
 * ---------------------------------------------------------------------------
 *
 * These functions are expected to be implemented by the olnk core.
 * They provide a stable way to find registered built-in or plugin-backed
 * formats from embedders and internal adapters.
 */

OLNK_API size_t OLNK_CALL olnk_format_count(void) OLNK_NOEXCEPT;

OLNK_API const olnk_format_definition_t* OLNK_CALL olnk_format_at(
    size_t index) OLNK_NOEXCEPT;

OLNK_API const olnk_format_definition_t* OLNK_CALL olnk_find_format(
    const char* name) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Helper utilities
 * ---------------------------------------------------------------------------
 */

static inline uint32_t
olnk_format_make_version(uint32_t major, uint32_t minor, uint32_t patch)
{
    return ((major & 0x3FFu) << 22) |
           ((minor & 0x3FFu) << 12) |
           ((patch & 0xFFFu) << 0);
}

static inline int
olnk_format_capability_test(uint32_t capabilities, uint32_t capability)
{
    return (capabilities & capability) == capability;
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OLNK_OLNK_FORMAT_H */
