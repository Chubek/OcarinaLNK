#ifndef OLNK_OLNK_IR_H
#define OLNK_OLNK_IR_H

/*
 * =============================================================================
 * IR (INTERMEDIATE REPRESENTATION) PUBLIC ABI
 * =============================================================================
 *
 * Purpose of this file:
 *   - Provide a *minimal*, relatively stable public view of the internal
 *     "linker IR" for:
 *       * inspection by tools (analyzers, visualizers, tests)
 *       * controlled extension by plugins / formats / machines
 *   - This is *not* a full, frozen definition of the linker’s internal data
 *     structures. It’s an abstraction the core can adapt to.
 *
 * Design goals:
 *   - C-compatible, no STL/C++ types.
 *   - Express only the *generic* concepts:
 *       * symbols, sections, segments, relocations, comdats/groups
 *       * image, object, and input-file level info
 *   - Ownership is always with the core; IR consumers do *not* mutate or free.
 *   - Capabilities are explicitly versioned and extensible (size + abi_version).
 *
 * Non-goals (for now):
 *   - Expose every format/machine-specific detail.
 *   - Provide full mutation APIs (this can be added later as an opt‑in layer).
 *
 * If you change the binary layout of exposed structs in a non‑backwards
 * compatible way, bump OLNK_IR_ABI_VERSION.
 * =============================================================================
 */

#include <stddef.h>
#include <stdint.h>

#include <olnk/olnk-api.h>
#include <olnk/olnk-version.h>
#include <olnk/olnk-format.h>
#include <olnk/olnk-machine.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * ---------------------------------------------------------------------------
 * IR ABI version
 * ---------------------------------------------------------------------------
 */

#define OLNK_IR_ABI_VERSION 1u

/*
 * ---------------------------------------------------------------------------
 * Forward declarations / opaque handles
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_ir_image        olnk_ir_image_t;
typedef struct olnk_ir_file         olnk_ir_file_t;
typedef struct olnk_ir_segment      olnk_ir_segment_t;
typedef struct olnk_ir_section      olnk_ir_section_t;
typedef struct olnk_ir_symbol       olnk_ir_symbol_t;
typedef struct olnk_ir_relocation   olnk_ir_relocation_t;
typedef struct olnk_ir_comdat       olnk_ir_comdat_t;
typedef struct olnk_ir_atom         olnk_ir_atom_t;   /* small unit of layout */
typedef struct olnk_ir_iterator     olnk_ir_iterator_t; /* opaque enumerator */

/*
 * ---------------------------------------------------------------------------
 * Core enums
 * ---------------------------------------------------------------------------
 */

typedef enum olnk_ir_file_kind {
    OLNK_IR_FILE_KIND_UNKNOWN = 0,
    OLNK_IR_FILE_KIND_OBJECT,
    OLNK_IR_FILE_KIND_SHARED_LIBRARY,
    OLNK_IR_FILE_KIND_ARCHIVE,
    OLNK_IR_FILE_KIND_WHOLE_ARCHIVE,
    OLNK_IR_FILE_KIND_SCRIPT,
    OLNK_IR_FILE_KIND_BITCODE,      /* e.g. LLVM IR */
    OLNK_IR_FILE_KIND_PSEUDO
} olnk_ir_file_kind_t;

typedef enum olnk_ir_section_kind {
    OLNK_IR_SECTION_KIND_UNKNOWN = 0,
    OLNK_IR_SECTION_KIND_CODE,
    OLNK_IR_SECTION_KIND_DATA,
    OLNK_IR_SECTION_KIND_BSS,
    OLNK_IR_SECTION_KIND_RODATA,
    OLNK_IR_SECTION_KIND_TLS,
    OLNK_IR_SECTION_KIND_DEBUG,
    OLNK_IR_SECTION_KIND_NOTE,
    OLNK_IR_SECTION_KIND_METADATA,
    OLNK_IR_SECTION_KIND_IMPORT_TABLE,
    OLNK_IR_SECTION_KIND_EXPORT_TABLE,
    OLNK_IR_SECTION_KIND_RELOCATION,
    OLNK_IR_SECTION_KIND_CUSTOM
} olnk_ir_section_kind_t;

typedef enum olnk_ir_symbol_binding {
    OLNK_IR_SYMBOL_BINDING_UNKNOWN = 0,
    OLNK_IR_SYMBOL_BINDING_LOCAL,
    OLNK_IR_SYMBOL_BINDING_GLOBAL,
    OLNK_IR_SYMBOL_BINDING_WEAK,
    OLNK_IR_SYMBOL_BINDING_EXTERN,
    OLNK_IR_SYMBOL_BINDING_PRIVATE
} olnk_ir_symbol_binding_t;

typedef enum olnk_ir_symbol_visibility {
    OLNK_IR_SYMBOL_VISIBILITY_DEFAULT = 0,
    OLNK_IR_SYMBOL_VISIBILITY_HIDDEN,
    OLNK_IR_SYMBOL_VISIBILITY_PROTECTED,
    OLNK_IR_SYMBOL_VISIBILITY_INTERNAL
} olnk_ir_symbol_visibility_t;

typedef enum olnk_ir_symbol_kind {
    OLNK_IR_SYMBOL_KIND_UNKNOWN = 0,
    OLNK_IR_SYMBOL_KIND_FUNCTION,
    OLNK_IR_SYMBOL_KIND_OBJECT,
    OLNK_IR_SYMBOL_KIND_TLS,
    OLNK_IR_SYMBOL_KIND_SECTION,
    OLNK_IR_SYMBOL_KIND_FILE,
    OLNK_IR_SYMBOL_KIND_ABSOLUTE,
    OLNK_IR_SYMBOL_KIND_COMMON,
    OLNK_IR_SYMBOL_KIND_IFUNC,
    OLNK_IR_SYMBOL_KIND_ALIAS
} olnk_ir_symbol_kind_t;

typedef enum olnk_ir_symbol_flags {
    OLNK_IR_SYMBOL_FLAG_NONE              = 0u,
    OLNK_IR_SYMBOL_FLAG_DEFINED           = 1u << 0,
    OLNK_IR_SYMBOL_FLAG_UNDEFINED         = 1u << 1,
    OLNK_IR_SYMBOL_FLAG_COMMON            = 1u << 2,
    OLNK_IR_SYMBOL_FLAG_WEAK              = 1u << 3,
    OLNK_IR_SYMBOL_FLAG_TLS               = 1u << 4,
    OLNK_IR_SYMBOL_FLAG_CONSTRUCTOR       = 1u << 5,
    OLNK_IR_SYMBOL_FLAG_DESTRUCTOR        = 1u << 6,
    OLNK_IR_SYMBOL_FLAG_EXPORTED          = 1u << 7,
    OLNK_IR_SYMBOL_FLAG_IMPORTED          = 1u << 8,
    OLNK_IR_SYMBOL_FLAG_DYNAMIC           = 1u << 9,
    OLNK_IR_SYMBOL_FLAG_INTERPOSE         = 1u << 10,
    OLNK_IR_SYMBOL_FLAG_CAN_BE_INLINE     = 1u << 11
} olnk_ir_symbol_flags_t;

typedef enum olnk_ir_relocation_kind {
    /*
     * Generic categories; specific formats/machines will have their own
     * detailed enumerations that map to these buckets. For now, this is
     * intentionally abstract.
     */
    OLNK_IR_RELOCATION_KIND_UNKNOWN = 0,
    OLNK_IR_RELOCATION_KIND_ABSOLUTE,
    OLNK_IR_RELOCATION_KIND_RELATIVE,
    OLNK_IR_RELOCATION_KIND_PLT,
    OLNK_IR_RELOCATION_KIND_GOT,
    OLNK_IR_RELOCATION_KIND_TLS,
    OLNK_IR_RELOCATION_KIND_COPY,
    OLNK_IR_RELOCATION_KIND_JUMP_SLOT,
    OLNK_IR_RELOCATION_KIND_IFUNC,
    OLNK_IR_RELOCATION_KIND_CUSTOM
} olnk_ir_relocation_kind_t;

/*
 * COMDAT/group semantics (weakly keyed section groups).
 */
typedef enum olnk_ir_comdat_kind {
    OLNK_IR_COMDAT_KIND_UNKNOWN = 0,
    OLNK_IR_COMDAT_KIND_ANY,
    OLNK_IR_COMDAT_KIND_EXACT_MATCH,
    OLNK_IR_COMDAT_KIND_LARGEST,
    OLNK_IR_COMDAT_KIND_NO_DUPLICATES,
    OLNK_IR_COMDAT_KIND_SAME_SIZE
} olnk_ir_comdat_kind_t;

/*
 * ---------------------------------------------------------------------------
 * Helper bitmasks and utilities
 * ---------------------------------------------------------------------------
 */

static inline int
olnk_ir_symbol_has_flag(uint32_t flags, uint32_t flag)
{
    return (flags & flag) == flag;
}

/*
 * ---------------------------------------------------------------------------
 * Basic POD metadata structs
 * ---------------------------------------------------------------------------
 */

typedef struct olnk_ir_range {
    uint64_t offset;  /* byte offset, relative to some base */
    uint64_t size;    /* size in bytes */
} olnk_ir_range_t;

/*
 * Per-input-file view.
 */
typedef struct olnk_ir_file_info {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* path;             /* full or logical path */
    const char* name;             /* basename, optional */

    olnk_ir_file_kind_t kind;

    uint32_t index;               /* logical index in image's file table */

    uint32_t is_system : 1;
    uint32_t is_lazy   : 1;
    uint32_t is_gc_root: 1;
    uint32_t reserved_flags : 29;

    uint32_t machine_kind;        /* olnk_machine_kind_t, stored as uint32_t */
    uint32_t format_kind;         /* olnk_format_kind_t, stored as uint32_t */

    void* user_data;              /* reserved for core use / plugins via host */

    void* reserved_0;
    void* reserved_1;
} olnk_ir_file_info_t;

typedef struct olnk_ir_section_info {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;
    const char* comdat_key;       /* NULL if not in a COMDAT group */

    olnk_ir_section_kind_t kind;
    uint32_t flags;               /* mapped from olnk_format_section_flags_t */

    uint32_t index;               /* section index in the image */
    uint32_t file_index;          /* originating file (olnk_ir_file_info_t.index) */

    olnk_ir_range_t file_range;   /* file offset + size */
    olnk_ir_range_t virt_range;   /* virtual address + size */

    uint64_t alignment;

    uint32_t is_alloc : 1;
    uint32_t is_load  : 1;
    uint32_t is_debug : 1;
    uint32_t reserved_bits : 29;

    void* reserved_0;
    void* reserved_1;
} olnk_ir_section_info_t;

typedef struct olnk_ir_symbol_info {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;
    const char* version;          /* symbol version, optional */

    olnk_ir_symbol_kind_t kind;
    olnk_ir_symbol_binding_t binding;
    olnk_ir_symbol_visibility_t visibility;

    uint32_t flags;               /* bitmask of olnk_ir_symbol_flags_t */

    uint32_t file_index;          /* defining (or referencing) file index */
    uint32_t section_index;       /* section index or sentinel for ABS/UNDEF */

    uint64_t value;               /* address or section-relative offset */
    uint64_t size;                /* symbol size in bytes */

    uint32_t is_defined : 1;
    uint32_t is_tls     : 1;
    uint32_t is_import  : 1;
    uint32_t is_export  : 1;
    uint32_t reserved_bits : 28;

    void* reserved_0;
    void* reserved_1;
} olnk_ir_symbol_info_t;

typedef struct olnk_ir_relocation_info {
    uint32_t abi_version;
    uint32_t struct_size;

    uint32_t file_index;          /* owning file index */
    uint32_t section_index;       /* owning section index */

    uint64_t offset;              /* byte offset within section */
    int64_t addend;               /* relocation addend (if applicable) */

    uint32_t symbol_index;        /* index into image symbol table */
    uint32_t type;                /* format/machine-specific encoded type */

    olnk_ir_relocation_kind_t kind;

    uint32_t is_pc_relative : 1;
    uint32_t is_tls         : 1;
    uint32_t is_plt         : 1;
    uint32_t reserved_bits  : 29;

    void* reserved_0;
    void* reserved_1;
} olnk_ir_relocation_info_t;

/*
 * COMDAT / group info.
 */
typedef struct olnk_ir_comdat_info {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* key;
    olnk_ir_comdat_kind_t kind;

    uint32_t file_index;          /* file providing the comdat group */
    uint32_t section_count;

    void* reserved_0;
    void* reserved_1;
} olnk_ir_comdat_info_t;

/*
 * Optional fine-grained atom (layout unit) info.
 * Not all backends need to expose atoms; if unsupported, counts may be zero.
 */
typedef struct olnk_ir_atom_info {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;             /* may be NULL */
    uint32_t file_index;
    uint32_t section_index;

    uint64_t offset;              /* section-relative */
    uint64_t size;

    uint64_t alignment;

    uint32_t is_dead      : 1;    /* GC has removed this atom */
    uint32_t is_gc_root   : 1;
    uint32_t is_hot       : 1;
    uint32_t is_cold      : 1;
    uint32_t reserved_bits: 28;

    void* reserved_0;
    void* reserved_1;
} olnk_ir_atom_info_t;

/*
 * Image-level summary.
 */
typedef struct olnk_ir_image_info {
    uint32_t abi_version;
    uint32_t struct_size;

    const char* name;             /* optional logical name */
    const char* output_path;      /* optional final path */

    uint32_t file_count;
    uint32_t section_count;
    uint32_t symbol_count;
    uint32_t relocation_count;
    uint32_t comdat_count;
    uint32_t atom_count;

    uint32_t machine_kind;        /* olnk_machine_kind_t (as uint32_t) */
    uint32_t format_kind;         /* olnk_format_kind_t (as uint32_t) */

    uint64_t image_base;
    uint64_t entry_address;

    uint32_t has_incremental_state : 1;
    uint32_t has_debug_info        : 1;
    uint32_t reserved_bits         : 30;

    void* reserved_0;
    void* reserved_1;
} olnk_ir_image_info_t;

/*
 * ---------------------------------------------------------------------------
 * Opaque IR iterator
 * ---------------------------------------------------------------------------
 *
 * An IR iterator is a generic enumerator for one "kind" of IR entity
 * (symbols, sections, relocations, etc.). It is host-owned and must
 * be destroyed when no longer needed.
 *
 * Iteration pattern:
 *   - Create iterator via appropriate API (e.g. olnk_ir_image_symbols()).
 *   - Call next() until it returns 0.
 *   - Destroy iterator.
 */

typedef enum olnk_ir_iterator_kind {
    OLNK_IR_ITERATOR_KIND_UNKNOWN = 0,
    OLNK_IR_ITERATOR_KIND_FILES,
    OLNK_IR_ITERATOR_KIND_SECTIONS,
    OLNK_IR_ITERATOR_KIND_SYMBOLS,
    OLNK_IR_ITERATOR_KIND_RELOCATIONS,
    OLNK_IR_ITERATOR_KIND_COMDATS,
    OLNK_IR_ITERATOR_KIND_ATOMS
} olnk_ir_iterator_kind_t;

/*
 * ---------------------------------------------------------------------------
 * IR host / context (for plugins/formats/machines)
 * ---------------------------------------------------------------------------
 *
 * For now, we keep this very small and read-only.
 */

typedef struct olnk_ir_host {
    uint32_t abi_version;
    uint32_t struct_size;

    void (OLNK_CALL *log)(
        olnk_context_t* context,
        olnk_log_level_t level,
        const char* message);

    void (OLNK_CALL *diagnostic)(
        olnk_context_t* context,
        olnk_diagnostic_severity_t severity,
        const char* message);

    void* reserved_0;
    void* reserved_1;
    void* reserved_2;
    void* reserved_3;
} olnk_ir_host_t;

/*
 * ---------------------------------------------------------------------------
 * Public IR access API (implemented by core)
 * ---------------------------------------------------------------------------
 *
 * All returned pointers are owned by the core and remain valid at least for
 * the lifetime of the associated session/image. Callers MUST NOT modify or
 * free them.
 */

/* Acquire a read-only IR image for the given session.
 * Returns NULL if IR is not available (e.g. streaming/link-only mode). */
OLNK_API const olnk_ir_image_t* OLNK_CALL
olnk_ir_get_image(olnk_session_t* session) OLNK_NOEXCEPT;

/* Query basic image info. Returns OLNK_STATUS_OK on success. */
OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_image_info(const olnk_ir_image_t* image,
                       olnk_ir_image_info_t* out_info) OLNK_NOEXCEPT;

/*
 * Direct indexed accessors
 *  - Indexes are 0 .. count-1 as reported by image info.
 *  - On out-of-range index, returns OLNK_STATUS_INVALID_ARGUMENT.
 */

OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_file_info(const olnk_ir_image_t* image,
                      uint32_t index,
                      olnk_ir_file_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_section_info(const olnk_ir_image_t* image,
                         uint32_t index,
                         olnk_ir_section_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_symbol_info(const olnk_ir_image_t* image,
                        uint32_t index,
                        olnk_ir_symbol_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_relocation_info(const olnk_ir_image_t* image,
                            uint32_t index,
                            olnk_ir_relocation_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_comdat_info(const olnk_ir_image_t* image,
                        uint32_t index,
                        olnk_ir_comdat_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_atom_info(const olnk_ir_image_t* image,
                      uint32_t index,
                      olnk_ir_atom_info_t* out_info) OLNK_NOEXCEPT;

/*
 * Iterators
 */

OLNK_API olnk_status_t OLNK_CALL
olnk_ir_iterator_create(const olnk_ir_image_t* image,
                        olnk_ir_iterator_kind_t kind,
                        olnk_ir_iterator_t** out_it) OLNK_NOEXCEPT;

OLNK_API void OLNK_CALL
olnk_ir_iterator_destroy(olnk_ir_iterator_t* it) OLNK_NOEXCEPT;

/* Returns 1 on success (and fills the appropriate *_info), 0 on end, or a
 * negative status code encoded as int on error (e.g., OLNK_STATUS_* cast). */
OLNK_API int OLNK_CALL
olnk_ir_iterator_next_file(olnk_ir_iterator_t* it,
                           olnk_ir_file_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API int OLNK_CALL
olnk_ir_iterator_next_section(olnk_ir_iterator_t* it,
                              olnk_ir_section_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API int OLNK_CALL
olnk_ir_iterator_next_symbol(olnk_ir_iterator_t* it,
                             olnk_ir_symbol_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API int OLNK_CALL
olnk_ir_iterator_next_relocation(olnk_ir_iterator_t* it,
                                 olnk_ir_relocation_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API int OLNK_CALL
olnk_ir_iterator_next_comdat(olnk_ir_iterator_t* it,
                             olnk_ir_comdat_info_t* out_info) OLNK_NOEXCEPT;

OLNK_API int OLNK_CALL
olnk_ir_iterator_next_atom(olnk_ir_iterator_t* it,
                           olnk_ir_atom_info_t* out_info) OLNK_NOEXCEPT;

/*
 * ---------------------------------------------------------------------------
 * Helper utilities
 * ---------------------------------------------------------------------------
 */

static inline uint32_t
olnk_ir_make_version(uint32_t major, uint32_t minor, uint32_t patch)
{
    return ((major & 0x3FFu) << 22) |
           ((minor & 0x3FFu) << 12) |
           ((patch & 0xFFFu) << 0);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* OLNK_OLNK_IR_H */
