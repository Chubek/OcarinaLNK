#ifndef OLNK_OLNK_MACHINE_H
#define OLNK_OLNK_MACHINE_H

/*
 * =============================================================================
 * LLM AGENT NOTES / PATCH GUIDANCE
 * =============================================================================
 *
 * Purpose of this file:
 *   - Define the public machine/architecture ABI for olnk.
 *   - A "machine" identifies the target execution architecture or virtual ISA
 *     for linking, relocation semantics, calling convention defaults, object
 *     compatibility checks, and output metadata.
 *   - Machines may be physical (x86_64, AArch64, RISC-V) or virtual
 *     (WASM32/WASM64, eBPF, custom VMs).
 *
 * Intended usage:
 *   - The core linker resolves a machine by name/triple/alias.
 *   - Formats and plugins may query machine properties.
 *   - Future backends may provide machine-specific relocation, relaxation,
 *     stub/veneer generation, TLS lowering, PLT/GOT synthesis, and branch range
 *     extension through this ABI.
 *
 * Current design goals:
 *   - Keep the ABI C-compatible and conservative.
 *   - Support both concrete hardware targets and virtual targets.
 *   - Expose generic properties useful to embedders without locking the linker
 *     into a specific internal IR.
 *   - Allow built-in and external machine definitions.
 *
 * Major future improvements:
 *   1. Add relocation-kind enumeration namespaces and translators.
 *   2. Add relaxation/veneer/thunk planning hooks.
 *   3. Add symbol ABI traits:
 *        - function descriptors
 *        - GOT/PLT requirements
 *        - TLS models
 *        - calling convention metadata
 *   4. Add sub-architecture / feature-bit parsing:
 *        - armv8.5-a + pauth
 *        - rv64gc + zba
 *        - x86-64-v3
 *   5. Add machine-specific section conventions and unwind/debug hooks.
 *   6. Add object input compatibility checks against file headers.
 *   7. Add target triple parsing helpers and canonicalization.
 *   8. Potentially split "ISA" from "ABI environment" if needed later.
 *
 * Constraints:
 *   - Keep strings and structs simple.
 *   - Avoid exposing unstable linker internals.
 *   - Prefer additive ABI evolution via struct size/version fields.
 *
 * Ownership rules:
 *   - Descriptor strings should generally be static/module-lifetime strings.
 *   - Lookup APIs return host-owned pointers.
 *
 * If patching:
 *   - Prefer additive changes.
 *   - If binary incompatibilities are introduced, bump
 *     OLNK_MACHINE_ABI_VERSION.
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
 * Machine ABI version
 * ---------------------------------------------------------------------------
 */

#define OLNK_MACHINE_ABI_VERSION 1u

/*
 * ---------------------------------------------------------------------------
 * Forward declarations / opaque handles
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_machine            olnk_machine_t;
typedef struct olnk_machine_instance   olnk_machine_instance_t;
typedef struct olnk_machine_context    olnk_machine_context_t;

/*
 * ---------------------------------------------------------------------------
 * Core enums
 * ---------------------------------------------------------------------------
 */

typedef enum olnk_machine_kind {
    OLNK_MACHINE_KIND_UNKNOWN = 0,

    /* x86 family */
    OLNK_MACHINE_KIND_I386 = 1,
    OLNK_MACHINE_KIND_X86_64 = 2,

    /* ARM family */
    OLNK_MACHINE_KIND_ARM = 10,
    OLNK_MACHINE_KIND_AARCH64 = 11,
    OLNK_MACHINE_KIND_THUMB = 12,

    /* RISC-V */
    OLNK_MACHINE_KIND_RISCV32 = 20,
    OLNK_MACHINE_KIND_RISCV64 = 21,

    /* Power / IBM */
    OLNK_MACHINE_KIND_PPC32 = 30,
    OLNK_MACHINE_KIND_PPC64 = 31,

    /* MIPS */
    OLNK_MACHINE_KIND_MIPS32 = 40,
    OLNK_MACHINE_KIND_MIPS64 = 41,

    /* SPARC */
    OLNK_MACHINE_KIND_SPARC32 = 50,
    OLNK_MACHINE_KIND_SPARC64 = 51,

    /* Other physical */
    OLNK_MACHINE_KIND_SYSTEMZ = 60,
    OLNK_MACHINE_KIND_LOONGARCH32 = 61,
    OLNK_MACHINE_KIND_LOONGARCH64 = 62,

    /* Virtual / sandboxed */
    OLNK_MACHINE_KIND_WASM32 = 100,
    OLNK_MACHINE_KIND_WASM64 = 101,
    OLNK_MACHINE_KIND_BPFEL = 102,
    OLNK_MACHINE_KIND_BPFEB = 103,

    /* Escape hatch */
    OLNK_MACHINE_KIND_CUSTOM = 255
} olnk_machine_kind_t;

typedef enum olnk_machine_endianness {
    OLNK_MACHINE_ENDIANNESS_UNKNOWN = 0,
    OLNK_MACHINE_ENDIANNESS_LITTLE = 1,
    OLNK_MACHINE_ENDIANNESS_BIG = 2
} olnk_machine_endianness_t;

typedef enum olnk_machine_address_class {
    OLNK_MACHINE_ADDRESS_CLASS_UNKNOWN = 0,
    OLNK_MACHINE_ADDRESS_CLASS_16 = 16,
    OLNK_MACHINE_ADDRESS_CLASS_32 = 32,
    OLNK_MACHINE_ADDRESS_CLASS_64 = 64,
    OLNK_MACHINE_ADDRESS_CLASS_128 = 128
} olnk_machine_address_class_t;

/*
 * Feature/capability hints. These are generic and intentionally advisory.
 */
typedef enum olnk_machine_capability {
    OLNK_MACHINE_CAPABILITY_NONE                 = 0u,
    OLNK_MACHINE_CAPABILITY_EXECUTABLE          = 1u << 0,
    OLNK_MACHINE_CAPABILITY_SHARED_LIBRARY      = 1u << 1,
    OLNK_MACHINE_CAPABILITY_RELOCATABLE_OUTPUT  = 1u << 2,
    OLNK_MACHINE_CAPABILITY_TLS                 = 1u << 3,
    OLNK_MACHINE_CAPABILITY_PIC                 = 1u << 4,
    OLNK_MACHINE_CAPABILITY_RELAXATION          = 1u << 5,
    OLNK_MACHINE_CAPABILITY_BRANCH_VENEERS      = 1u << 6,
    OLNK_MACHINE_CAPABILITY_GOT                 = 1u << 7,
    OLNK_MACHINE_CAPABILITY_PLT                 = 1u << 8,
    OLNK_MACHINE_CAPABILITY_ATOMICS            = 1u << 9,
    OLNK_MACHINE_CAPABILITY_SIMD               = 1u << 10,
    OLNK_MACHINE_CAPABILITY_UNALIGNED_ACCESS   = 1u << 11,
    OLNK_MACHINE_CAPABILITY_VIRTUAL_ISA        = 1u << 12
} olnk_machine_capability_t;

/*
 * Generic relocation model / code model hints.
 * These are not meant to replace machine-specific ABI rules.
 */
typedef enum olnk_machine_code_model {
    OLNK_MACHINE_CODE_MODEL_UNKNOWN = 0,
    OLNK_MACHINE_CODE_MODEL_TINY = 1,
    OLNK_MACHINE_CODE_MODEL_SMALL = 2,
    OLNK_MACHINE_CODE_MODEL_MEDIUM = 3,
    OLNK_MACHINE_CODE_MODEL_LARGE = 4
} olnk_machine_code_model_t;

/*
 * ---------------------------------------------------------------------------
 * Machine properties
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_machine_properties {
    uint32_t abi_version;
    uint32_t struct_size;

    olnk_machine_kind_t kind;
    uint32_t capabilities; /* bitmask of olnk_machine_capability_t */

    olnk_machine_endianness_t endianness;
    olnk_machine_address_class_t address_class;

    /*
     * Canonical sizes in bytes. Zero means "not applicable / unknown".
     */
    uint32_t pointer_size;
    uint32_t address_size;
    uint32_t instruction_alignment;
    uint32_t stack_alignment;
    uint32_t function_alignment;

    /*
     * Generic defaults. Zero means implementation-defined / not specified.
     */
    uint64_t default_image_base;
    uint64_t default_page_size;
    uint64_t max_branch_range;

    olnk_machine_code_model_t default_code_model;

    /*
     * Optional canonical names for common metadata.
     */
    const char* canonical_name;      /* e.g. "aarch64" */
    const char* canonical_triple;    /* e.g. "aarch64-unknown-none" */

    void* reserved_0;
    void* reserved_1;
} olnk_machine_properties_t;

/*
 * ---------------------------------------------------------------------------
 * Machine host service API
 * ---------------------------------------------------------------------------
 *
 * Future revisions can extend this with relocation/object/layout services.
 */

typedef struct olnk_machine_host {
    uint32_t abi_version;
    uint32_t struct_size;

    void (OLNK_CALL *log)(
        olnk_machine_context_t* machine_ctx,
        olnk_log_level_t level,
        const char* message);

    void (OLNK_CALL *diagnostic)(
        olnk_machine_context_t* machine_ctx,
        olnk_diagnostic_severity_t severity,
        const char* message);

    const char* (OLNK_CALL *get_option)(
        olnk_machine_context_t* machine_ctx,
        const char* key);

    olnk_status_t (OLNK_CALL *set_option)(
        olnk_machine_context_t* machine_ctx,
        const char* key,
        const char* value);

    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
} olnk_machine_host_t;

/*
 * ---------------------------------------------------------------------------
 * Machine execution context
 * ---------------------------------------------------------------------------
 */

struct olnk_machine_context {
    const olnk_machine_host_t* host;
    olnk_context_t* linker_context;
    olnk_session_t* session;
    const olnk_config_t* config;

    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
};

/*
 * ---------------------------------------------------------------------------
 * Machine descriptor
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_machine_descriptor {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;          /* display name: "AArch64", "WASM32" */
    const char* description;
    const char* vendor;
    const char* license;

    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;

    olnk_machine_kind_t kind;
    uint32_t capabilities; /* bitmask of olnk_machine_capability_t */

    uint32_t min_host_api_version;
    uint32_t max_host_api_version;

    /*
     * Common aliases:
     *   "arm64", "aarch64", "x64", "wasm", "wasm32", etc.
     * Optional and host-interpreted.
     */
    const char* primary_alias;
    const char* secondary_alias;
    const char* canonical_triple;

    void* reserved_0;
    void* reserved_1;
} olnk_machine_descriptor_t;

/*
 * ---------------------------------------------------------------------------
 * Machine callback table
 * ---------------------------------------------------------------------------
 *
 * All callbacks are optional unless otherwise documented, but practical
 * implementations should generally provide:
 *   - create
 *   - destroy
 *   - get_properties
 *   - validate_config
 */

typedef struct olnk_machine_vtable {
    uint32_t abi_version;
    uint32_t struct_size;

    olnk_status_t (OLNK_CALL *create)(
        const olnk_machine_descriptor_t* descriptor,
        olnk_machine_instance_t** out_instance);

    void (OLNK_CALL *destroy)(
        olnk_machine_instance_t* instance);

    /*
     * Optional per-instance configuration for machine-specific tuning:
     *   cpu=cortex-a76
     *   features=+simd,+crypto
     *   abi=lp64d
     *   code-model=small
     */
    olnk_status_t (OLNK_CALL *set_option)(
        olnk_machine_instance_t* instance,
        const char* key,
        const char* value);

    /*
     * Query generic machine properties.
     */
    olnk_status_t (OLNK_CALL *get_properties)(
        olnk_machine_instance_t* instance,
        olnk_machine_properties_t* out_properties);

    /*
     * Validate configuration and machine-specific constraints.
     * Implementations may emit diagnostics via the host.
     */
    olnk_status_t (OLNK_CALL *validate_config)(
        olnk_machine_instance_t* instance,
        olnk_machine_context_t* context);

    /*
     * Optional normalization helpers.
     */
    const char* (OLNK_CALL *canonical_name)(
        const olnk_machine_instance_t* instance);

    const char* (OLNK_CALL *canonical_triple)(
        const olnk_machine_instance_t* instance);

    /*
     * Optional user-facing description for diagnostics/tooling.
     * Returned pointer is owned by the machine/module and should remain valid
     * at least for the instance lifetime.
     */
    const char* (OLNK_CALL *describe)(
        const olnk_machine_instance_t* instance);

    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
} olnk_machine_vtable_t;

/*
 * ---------------------------------------------------------------------------
 * Machine definition object
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_machine_definition {
    uint32_t abi_version;
    uint32_t struct_size;

    const olnk_machine_descriptor_t* descriptor;
    const olnk_machine_vtable_t* vtable;

    const void* module_data;

    void* reserved_0;
    void* reserved_1;
} olnk_machine_definition_t;

/*
 * ---------------------------------------------------------------------------
 * Public lookup / registry APIs
 * ---------------------------------------------------------------------------
 */

OLNK_API size_t OLNK_CALL olnk_machine_count(void) OLNK_NOEXCEPT;

OLNK_API const olnk_machine_definition_t* OLNK_CALL olnk_machine_at(
    size_t index) OLNK_NOEXCEPT;

OLNK_API const olnk_machine_definition_t* OLNK_CALL olnk_find_machine(
    const char* name) OLNK_NOEXCEPT;

OLNK_API const olnk_machine_definition_t* OLNK_CALL olnk_find_machine_by_kind(
    olnk_machine_kind_t kind) OLNK_NOEXCEPT;

OLNK_API const char* OLNK_CALL olnk_machine_kind_name(
    olnk_machine_kind_t kind) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Helper utilities
 * ---------------------------------------------------------------------------
 */

static inline uint32_t
olnk_machine_make_version(uint32_t major, uint32_t minor, uint32_t patch)
{
    return ((major & 0x3FFu) << 22) |
           ((minor & 0x3FFu) << 12) |
           ((patch & 0xFFFu) << 0);
}

static inline int
olnk_machine_capability_test(uint32_t capabilities, uint32_t capability)
{
    return (capabilities & capability) == capability;
}

static inline int
olnk_machine_is_virtual_kind(olnk_machine_kind_t kind)
{
    switch (kind) {
        case OLNK_MACHINE_KIND_WASM32:
        case OLNK_MACHINE_KIND_WASM64:
        case OLNK_MACHINE_KIND_BPFEL:
        case OLNK_MACHINE_KIND_BPFEB:
            return 1;
        default:
            return 0;
    }
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OLNK_OLNK_MACHINE_H */
