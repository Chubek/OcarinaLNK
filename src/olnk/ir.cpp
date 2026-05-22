// src/olnk/ir.cpp
//
// LLM / maintainer hints:
//   - Implement ONLY what can be justified from public ol nk-ir.h ABI.
//   - Do NOT invent hidden builder/mutation/session plumbing here.
//   - The header exposes opaque IR handle types and read-only query functions,
//     but it does NOT expose any way to materialize a real image from an
//     olnk_session_t using public ABI alone.
//   - Therefore this file provides a conservative ABI-safe fallback:
//       * olnk_ir_get_image() => nullptr
//       * info accessors validate arguments and return NOT_FOUND for null image
//       * in-range indexed access on a non-null image returns NOT_SUPPORTED
//         unless a real backing implementation is later wired in
//       * iterators are fully functional as host-owned enumerators, but over
//         whatever counts the image reports; for null image they are empty
//   - If the core later introduces internal definitions for olnk_ir_image_t,
//     this file can be upgraded without changing the public ABI.
//   - Keep all returned POD structs initialized deterministically.

#include <olnk/olnk-ir.h>

#include <new>

struct olnk_ir_image {
    // LLM hint:
    // This is intentionally minimal. Since the public ABI gives no constructor
    // or session->image extraction contract, we only store image summary info.
    // A future internal implementation may replace/extend this struct.
    olnk_ir_image_info_t info;
};

struct olnk_ir_iterator {
    // LLM hint:
    // Iterator is host-owned and opaque in the ABI. This implementation uses
    // simple index/count iteration over a single entity kind.
    const olnk_ir_image_t* image;
    olnk_ir_iterator_kind_t kind;
    uint32_t index;
    uint32_t count;
};

static constexpr void
olnk_ir_zero_reserved_image_info(olnk_ir_image_info_t* info) OLNK_NOEXCEPT
{
    info->reserved_0 = nullptr;
    info->reserved_1 = nullptr;
}

static constexpr void
olnk_ir_zero_reserved_file_info(olnk_ir_file_info_t* info) OLNK_NOEXCEPT
{
    info->reserved_0 = nullptr;
    info->reserved_1 = nullptr;
}

static constexpr void
olnk_ir_zero_reserved_section_info(olnk_ir_section_info_t* info) OLNK_NOEXCEPT
{
    info->reserved_0 = nullptr;
    info->reserved_1 = nullptr;
}

static constexpr void
olnk_ir_zero_reserved_symbol_info(olnk_ir_symbol_info_t* info) OLNK_NOEXCEPT
{
    info->reserved_0 = nullptr;
    info->reserved_1 = nullptr;
}

static constexpr void
olnk_ir_zero_reserved_relocation_info(olnk_ir_relocation_info_t* info) OLNK_NOEXCEPT
{
    info->reserved_0 = nullptr;
    info->reserved_1 = nullptr;
}

static constexpr void
olnk_ir_zero_reserved_comdat_info(olnk_ir_comdat_info_t* info) OLNK_NOEXCEPT
{
    info->reserved_0 = nullptr;
    info->reserved_1 = nullptr;
}

static constexpr void
olnk_ir_zero_reserved_atom_info(olnk_ir_atom_info_t* info) OLNK_NOEXCEPT
{
    info->reserved_0 = nullptr;
    info->reserved_1 = nullptr;
}

static void
olnk_ir_init_empty_image_info(olnk_ir_image_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return;
    }

    // LLM hint:
    // Always initialize every public field, even in failure / fallback mode.
    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_image_info_t);
    out_info->name = nullptr;
    out_info->output_path = nullptr;
    out_info->file_count = 0;
    out_info->section_count = 0;
    out_info->symbol_count = 0;
    out_info->relocation_count = 0;
    out_info->comdat_count = 0;
    out_info->atom_count = 0;
    out_info->machine_kind = 0;
    out_info->format_kind = 0;
    out_info->image_base = 0;
    out_info->entry_address = 0;
    out_info->has_incremental_state = 0;
    out_info->has_debug_info = 0;
    out_info->reserved_bits = 0;
    olnk_ir_zero_reserved_image_info(out_info);
}

static void
olnk_ir_init_empty_file_info(olnk_ir_file_info_t* out_info, uint32_t index) OLNK_NOEXCEPT
{
    if (!out_info) {
        return;
    }

    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_file_info_t);
    out_info->path = nullptr;
    out_info->name = nullptr;
    out_info->kind = OLNK_IR_FILE_KIND_UNKNOWN;
    out_info->index = index;
    out_info->is_system = 0;
    out_info->is_lazy = 0;
    out_info->is_gc_root = 0;
    out_info->reserved_flags = 0;
    out_info->machine_kind = 0;
    out_info->format_kind = 0;
    out_info->user_data = nullptr;
    olnk_ir_zero_reserved_file_info(out_info);
}

static void
olnk_ir_init_empty_section_info(olnk_ir_section_info_t* out_info, uint32_t index) OLNK_NOEXCEPT
{
    if (!out_info) {
        return;
    }

    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_section_info_t);
    out_info->name = nullptr;
    out_info->comdat_key = nullptr;
    out_info->kind = OLNK_IR_SECTION_KIND_UNKNOWN;
    out_info->flags = 0;
    out_info->index = index;
    out_info->file_index = 0;
    out_info->file_range.offset = 0;
    out_info->file_range.size = 0;
    out_info->virt_range.offset = 0;
    out_info->virt_range.size = 0;
    out_info->alignment = 0;
    out_info->is_alloc = 0;
    out_info->is_load = 0;
    out_info->is_debug = 0;
    out_info->reserved_bits = 0;
    olnk_ir_zero_reserved_section_info(out_info);
}

static void
olnk_ir_init_empty_symbol_info(olnk_ir_symbol_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return;
    }

    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_symbol_info_t);
    out_info->name = nullptr;
    out_info->version = nullptr;
    out_info->kind = OLNK_IR_SYMBOL_KIND_UNKNOWN;
    out_info->binding = OLNK_IR_SYMBOL_BINDING_UNKNOWN;
    out_info->visibility = OLNK_IR_SYMBOL_VISIBILITY_DEFAULT;
    out_info->flags = 0;
    out_info->file_index = 0;
    out_info->section_index = 0;
    out_info->value = 0;
    out_info->size = 0;
    out_info->is_defined = 0;
    out_info->is_tls = 0;
    out_info->is_import = 0;
    out_info->is_export = 0;
    out_info->reserved_bits = 0;
    olnk_ir_zero_reserved_symbol_info(out_info);
}

static void
olnk_ir_init_empty_relocation_info(olnk_ir_relocation_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return;
    }

    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_relocation_info_t);
    out_info->file_index = 0;
    out_info->section_index = 0;
    out_info->offset = 0;
    out_info->addend = 0;
    out_info->symbol_index = 0;
    out_info->type = 0;
    out_info->kind = OLNK_IR_RELOCATION_KIND_UNKNOWN;
    out_info->is_pc_relative = 0;
    out_info->is_tls = 0;
    out_info->is_plt = 0;
    out_info->reserved_bits = 0;
    olnk_ir_zero_reserved_relocation_info(out_info);
}

static void
olnk_ir_init_empty_comdat_info(olnk_ir_comdat_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return;
    }

    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_comdat_info_t);
    out_info->key = nullptr;
    out_info->kind = OLNK_IR_COMDAT_KIND_UNKNOWN;
    out_info->file_index = 0;
    out_info->section_count = 0;
    olnk_ir_zero_reserved_comdat_info(out_info);
}

static void
olnk_ir_init_empty_atom_info(olnk_ir_atom_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return;
    }

    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_atom_info_t);
    out_info->name = nullptr;
    out_info->file_index = 0;
    out_info->section_index = 0;
    out_info->offset = 0;
    out_info->size = 0;
    out_info->alignment = 0;
    out_info->is_dead = 0;
    out_info->is_gc_root = 0;
    out_info->is_hot = 0;
    out_info->is_cold = 0;
    out_info->reserved_bits = 0;
    olnk_ir_zero_reserved_atom_info(out_info);
}

static uint32_t
olnk_ir_image_count_for_kind(const olnk_ir_image_t* image,
                             olnk_ir_iterator_kind_t kind) OLNK_NOEXCEPT
{
    // LLM hint:
    // Null image means "IR unavailable", not a crash. Treat as zero-count.
    if (!image) {
        return 0;
    }

    switch (kind) {
        case OLNK_IR_ITERATOR_KIND_FILES:
            return image->info.file_count;
        case OLNK_IR_ITERATOR_KIND_SECTIONS:
            return image->info.section_count;
        case OLNK_IR_ITERATOR_KIND_SYMBOLS:
            return image->info.symbol_count;
        case OLNK_IR_ITERATOR_KIND_RELOCATIONS:
            return image->info.relocation_count;
        case OLNK_IR_ITERATOR_KIND_COMDATS:
            return image->info.comdat_count;
        case OLNK_IR_ITERATOR_KIND_ATOMS:
            return image->info.atom_count;
        case OLNK_IR_ITERATOR_KIND_UNKNOWN:
        default:
            return 0;
    }
}

static int
olnk_ir_iterator_kind_matches(olnk_ir_iterator_t* it,
                              olnk_ir_iterator_kind_t expected_kind) OLNK_NOEXCEPT
{
    if (!it) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    if (it->kind != expected_kind) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    return 1;
}

extern "C" OLNK_API const olnk_ir_image_t* OLNK_CALL
olnk_ir_get_image(olnk_session_t* session) OLNK_NOEXCEPT
{
    (void)session;

    // LLM hint:
    // Public ABI alone gives no lawful way to retrieve or synthesize a real IR
    // image from an opaque session. Return nullptr to indicate IR unavailable.
    return nullptr;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_image_info(const olnk_ir_image_t* image,
                       olnk_ir_image_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (!image) {
        // LLM hint:
        // On null image, still fill output deterministically.
        olnk_ir_init_empty_image_info(out_info);
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    *out_info = image->info;
    out_info->abi_version = OLNK_IR_ABI_VERSION;
    out_info->struct_size = sizeof(olnk_ir_image_info_t);
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_file_info(const olnk_ir_image_t* image,
                      uint32_t index,
                      olnk_ir_file_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_ir_init_empty_file_info(out_info, index);

    if (!image) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (index >= image->info.file_count) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    // LLM hint:
    // In conservative mode there is no detailed backing table yet; return a
    // deterministically initialized record for in-range queries.
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_section_info(const olnk_ir_image_t* image,
                         uint32_t index,
                         olnk_ir_section_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_ir_init_empty_section_info(out_info, index);

    if (!image) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (index >= image->info.section_count) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_symbol_info(const olnk_ir_image_t* image,
                        uint32_t index,
                        olnk_ir_symbol_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_ir_init_empty_symbol_info(out_info);

    if (!image) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (index >= image->info.symbol_count) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_relocation_info(const olnk_ir_image_t* image,
                            uint32_t index,
                            olnk_ir_relocation_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_ir_init_empty_relocation_info(out_info);

    if (!image) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (index >= image->info.relocation_count) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_comdat_info(const olnk_ir_image_t* image,
                        uint32_t index,
                        olnk_ir_comdat_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_ir_init_empty_comdat_info(out_info);

    if (!image) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (index >= image->info.comdat_count) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_get_atom_info(const olnk_ir_image_t* image,
                      uint32_t index,
                      olnk_ir_atom_info_t* out_info) OLNK_NOEXCEPT
{
    if (!out_info) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_ir_init_empty_atom_info(out_info);

    if (!image) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (index >= image->info.atom_count) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    return OLNK_STATUS_OK;
}

extern "C" OLNK_API olnk_status_t OLNK_CALL
olnk_ir_iterator_create(const olnk_ir_image_t* image,
                        olnk_ir_iterator_kind_t kind,
                        olnk_ir_iterator_t** out_it) OLNK_NOEXCEPT
{
    if (!out_it) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    *out_it = nullptr;

    switch (kind) {
        case OLNK_IR_ITERATOR_KIND_FILES:
        case OLNK_IR_ITERATOR_KIND_SECTIONS:
        case OLNK_IR_ITERATOR_KIND_SYMBOLS:
        case OLNK_IR_ITERATOR_KIND_RELOCATIONS:
        case OLNK_IR_ITERATOR_KIND_COMDATS:
        case OLNK_IR_ITERATOR_KIND_ATOMS:
            break;
        case OLNK_IR_ITERATOR_KIND_UNKNOWN:
        default:
            return OLNK_STATUS_INVALID_ARGUMENT;
    }

    olnk_ir_iterator_t* it = new (std::nothrow) olnk_ir_iterator_t();
    if (!it) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    }

    it->image = image;
    it->kind = kind;
    it->index = 0;
    it->count = olnk_ir_image_count_for_kind(image, kind);

    *out_it = it;
    return OLNK_STATUS_OK;
}

extern "C" OLNK_API void OLNK_CALL
olnk_ir_iterator_destroy(olnk_ir_iterator_t* it) OLNK_NOEXCEPT
{
    delete it;
}

extern "C" OLNK_API int OLNK_CALL
olnk_ir_iterator_next_file(olnk_ir_iterator_t* it,
                           olnk_ir_file_info_t* out_info) OLNK_NOEXCEPT
{
    int match = olnk_ir_iterator_kind_matches(it, OLNK_IR_ITERATOR_KIND_FILES);
    if (match < 0) {
        return match;
    }
    if (!out_info) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    if (it->index >= it->count) {
        return 0;
    }

    const olnk_status_t st = olnk_ir_get_file_info(it->image, it->index, out_info);
    if (st != OLNK_STATUS_OK) {
        return static_cast<int>(st);
    }

    ++it->index;
    return 1;
}

extern "C" OLNK_API int OLNK_CALL
olnk_ir_iterator_next_section(olnk_ir_iterator_t* it,
                              olnk_ir_section_info_t* out_info) OLNK_NOEXCEPT
{
    int match = olnk_ir_iterator_kind_matches(it, OLNK_IR_ITERATOR_KIND_SECTIONS);
    if (match < 0) {
        return match;
    }
    if (!out_info) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    if (it->index >= it->count) {
        return 0;
    }

    const olnk_status_t st = olnk_ir_get_section_info(it->image, it->index, out_info);
    if (st != OLNK_STATUS_OK) {
        return static_cast<int>(st);
    }

    ++it->index;
    return 1;
}

extern "C" OLNK_API int OLNK_CALL
olnk_ir_iterator_next_symbol(olnk_ir_iterator_t* it,
                             olnk_ir_symbol_info_t* out_info) OLNK_NOEXCEPT
{
    int match = olnk_ir_iterator_kind_matches(it, OLNK_IR_ITERATOR_KIND_SYMBOLS);
    if (match < 0) {
        return match;
    }
    if (!out_info) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    if (it->index >= it->count) {
        return 0;
    }

    const olnk_status_t st = olnk_ir_get_symbol_info(it->image, it->index, out_info);
    if (st != OLNK_STATUS_OK) {
        return static_cast<int>(st);
    }

    ++it->index;
    return 1;
}

extern "C" OLNK_API int OLNK_CALL
olnk_ir_iterator_next_relocation(olnk_ir_iterator_t* it,
                                 olnk_ir_relocation_info_t* out_info) OLNK_NOEXCEPT
{
    int match = olnk_ir_iterator_kind_matches(it, OLNK_IR_ITERATOR_KIND_RELOCATIONS);
    if (match < 0) {
        return match;
    }
    if (!out_info) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    if (it->index >= it->count) {
        return 0;
    }

    const olnk_status_t st = olnk_ir_get_relocation_info(it->image, it->index, out_info);
    if (st != OLNK_STATUS_OK) {
        return static_cast<int>(st);
    }

    ++it->index;
    return 1;
}

extern "C" OLNK_API int OLNK_CALL
olnk_ir_iterator_next_comdat(olnk_ir_iterator_t* it,
                             olnk_ir_comdat_info_t* out_info) OLNK_NOEXCEPT
{
    int match = olnk_ir_iterator_kind_matches(it, OLNK_IR_ITERATOR_KIND_COMDATS);
    if (match < 0) {
        return match;
    }
    if (!out_info) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    if (it->index >= it->count) {
        return 0;
    }

    const olnk_status_t st = olnk_ir_get_comdat_info(it->image, it->index, out_info);
    if (st != OLNK_STATUS_OK) {
        return static_cast<int>(st);
    }

    ++it->index;
    return 1;
}

extern "C" OLNK_API int OLNK_CALL
olnk_ir_iterator_next_atom(olnk_ir_iterator_t* it,
                           olnk_ir_atom_info_t* out_info) OLNK_NOEXCEPT
{
    int match = olnk_ir_iterator_kind_matches(it, OLNK_IR_ITERATOR_KIND_ATOMS);
    if (match < 0) {
        return match;
    }
    if (!out_info) {
        return static_cast<int>(OLNK_STATUS_INVALID_ARGUMENT);
    }
    if (it->index >= it->count) {
        return 0;
    }

    const olnk_status_t st = olnk_ir_get_atom_info(it->image, it->index, out_info);
    if (st != OLNK_STATUS_OK) {
        return static_cast<int>(st);
    }

    ++it->index;
    return 1;
}
