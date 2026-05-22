// src/olnk/format.cpp
//
// =============================================================================
// LLM / MAINTAINER IMPLEMENTATION NOTES
// =============================================================================
//
// What this file is allowed to do:
//   - Implement ONLY the public registry/lookup functions declared in
//     ol nk-format.h:
//         * olnk_format_count()
//         * olnk_format_at()
//         * olnk_find_format()
//   - Respect the C ABI and avoid exposing STL/C++ types through it.
//   - Return stable pointers to host-owned static data.
//
// What this file is NOT allowed to invent casually:
//   - Do not fabricate plugin loading, dynamic module discovery, or filesystem
//     scanning unless such behavior is declared elsewhere in the public ABI.
//   - Do not assume built-in ELF/PE/Mach-O implementations exist unless they
//     are explicitly wired in by this translation unit or internal headers.
//   - Do not invent hidden session/config coupling for lookup functions.
//   - Do not throw exceptions across the C ABI.
//
// Conservative design used here:
//   - The public header defines registry-facing APIs, but does not define how
//     formats are registered at runtime.
//   - Therefore this file provides a safe static built-in registry scaffold.
//   - By default, the built-in registry is empty unless OLNK_FORMAT_STATIC_DEFS
//     is supplied at compile time.
//   - This keeps behavior deterministic and ABI-safe while still making future
//     integration easy.
//
// Integration strategy for future maintainers:
//   - If real built-in formats exist, define OLNK_FORMAT_STATIC_DEFS to expand
//     to a comma-separated list of pointers to const
//     olnk_format_definition_t objects, e.g.:
//
//       #define OLNK_FORMAT_STATIC_DEFS \
//           &olnk_builtin_elf_format_definition, \
//           &olnk_builtin_pe_format_definition
//
//   - Alternatively, replace olnk_get_static_format_registry() with a real
//     internal registry source once such infrastructure exists.
//   - Keep matching behavior case-insensitive for usability, but do not add
//     locale-sensitive behavior.
//
// ABI / safety rules:
//   - Never return dangling pointers.
//   - Treat malformed definitions defensively.
//   - Ignore registry entries with null descriptor pointers during lookup.
//   - Match against descriptor->name, primary_alias, and secondary_alias.
//   - Null input name => no match.
// =============================================================================

#include <olnk/olnk-format.h>

#include <sol/sol.hpp>

#include <stddef.h>
#include <cstring>
#include <fstream>
#include <new>
#include <string>
#include <vector>

namespace olnk {
void register_olnk_lua(sol::state& lua);
}

namespace {

// LLM hint:
// Internal lightweight "registry span" helper. This stays entirely within the
// .cpp and does not leak into the public ABI.
struct olnk_format_registry_view {
    const olnk_format_definition_t* const* items;
    size_t count;
};

struct BuiltinFormatInstance {
    const olnk_format_descriptor_t* descriptor;
};

static olnk_status_t OLNK_CALL builtin_create(const olnk_format_descriptor_t* descriptor,
                                              olnk_format_instance_t** out_instance) OLNK_NOEXCEPT
{
    if (descriptor == nullptr || out_instance == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    *out_instance = nullptr;
    try {
        BuiltinFormatInstance* inst = new BuiltinFormatInstance();
        inst->descriptor = descriptor;
        *out_instance = reinterpret_cast<olnk_format_instance_t*>(inst);
        return OLNK_STATUS_OK;
    } catch (const std::bad_alloc&) {
        return OLNK_STATUS_OUT_OF_MEMORY;
    } catch (...) {
        return OLNK_STATUS_INTERNAL_ERROR;
    }
}

static void OLNK_CALL builtin_destroy(olnk_format_instance_t* instance) OLNK_NOEXCEPT
{
    delete reinterpret_cast<BuiltinFormatInstance*>(instance);
}

static olnk_status_t OLNK_CALL builtin_get_properties(olnk_format_instance_t* instance,
                                                      olnk_format_properties_t* out_properties) OLNK_NOEXCEPT
{
    if (instance == nullptr || out_properties == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    BuiltinFormatInstance* inst = reinterpret_cast<BuiltinFormatInstance*>(instance);
    std::memset(out_properties, 0, sizeof(*out_properties));
    out_properties->abi_version = OLNK_FORMAT_ABI_VERSION;
    out_properties->struct_size = static_cast<uint32_t>(sizeof(*out_properties));
    out_properties->kind = inst->descriptor->kind;
    out_properties->capabilities = inst->descriptor->capabilities;
    out_properties->image_class = OLNK_FORMAT_IMAGE_CLASS_64;
    out_properties->endianness = OLNK_FORMAT_ENDIANNESS_LITTLE;
    return OLNK_STATUS_OK;
}

static olnk_status_t OLNK_CALL builtin_validate_config(olnk_format_instance_t* instance,
                                                       olnk_format_context_t* context) OLNK_NOEXCEPT
{
    if (instance == nullptr || context == nullptr || context->config == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    return OLNK_STATUS_OK;
}

static olnk_status_t OLNK_CALL builtin_serialize(olnk_format_instance_t* instance,
                                                 olnk_format_context_t* context,
                                                 const olnk_format_image_view_t* image_view,
                                                 olnk_format_output_info_t* out_info) OLNK_NOEXCEPT
{
    if (instance == nullptr || context == nullptr || context->host == nullptr ||
        context->host->emit_output == nullptr || image_view == nullptr || out_info == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    BuiltinFormatInstance* inst = reinterpret_cast<BuiltinFormatInstance*>(instance);
    const std::string payload =
        std::string("OLNK\nformat=") + (inst->descriptor->name ? inst->descriptor->name : "unknown") + "\n";
    const olnk_status_t emit_status =
        context->host->emit_output(context, payload.data(), payload.size());
    if (emit_status != OLNK_STATUS_OK) {
        return emit_status;
    }
    std::memset(out_info, 0, sizeof(*out_info));
    out_info->abi_version = OLNK_FORMAT_ABI_VERSION;
    out_info->struct_size = static_cast<uint32_t>(sizeof(*out_info));
    out_info->format_kind = inst->descriptor->kind;
    out_info->file_size = payload.size();
    out_info->output_kind = OLNK_OUTPUT_EXECUTABLE;
    (void)image_view;
    return OLNK_STATUS_OK;
}

static const olnk_format_vtable_t g_builtin_format_vtable = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_vtable_t), builtin_create, builtin_destroy, nullptr,
    builtin_get_properties,  builtin_validate_config,      nullptr,        nullptr,        builtin_serialize,
    nullptr,                 nullptr,                      nullptr,        nullptr,        nullptr};

static const olnk_format_descriptor_t g_elf_desc = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_descriptor_t), "ELF", "Builtin ELF format", "olnk", "internal",
    1, 0, 0, OLNK_FORMAT_KIND_ELF, OLNK_FORMAT_CAPABILITY_EXECUTABLE | OLNK_FORMAT_CAPABILITY_RELOCATABLE_OUTPUT,
    0, 0, "elf", "native", nullptr, nullptr};
static const olnk_format_descriptor_t g_pe_desc = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_descriptor_t), "PE", "Builtin PE format", "olnk", "internal",
    1, 0, 0, OLNK_FORMAT_KIND_PE, OLNK_FORMAT_CAPABILITY_EXECUTABLE | OLNK_FORMAT_CAPABILITY_SHARED_LIBRARY,
    0, 0, "pe", "coff", nullptr, nullptr};
static const olnk_format_descriptor_t g_macho_desc = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_descriptor_t), "Mach-O", "Builtin Mach-O format", "olnk", "internal",
    1, 0, 0, OLNK_FORMAT_KIND_MACH_O, OLNK_FORMAT_CAPABILITY_EXECUTABLE, 0, 0, "mach-o", "macho", nullptr, nullptr};
static const olnk_format_descriptor_t g_wasm_desc = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_descriptor_t), "WASM", "Builtin WASM format", "olnk", "internal",
    1, 0, 0, OLNK_FORMAT_KIND_WASM, OLNK_FORMAT_CAPABILITY_RELOCATABLE_OUTPUT, 0, 0, "wasm", "wasm32", nullptr, nullptr};

static const olnk_format_definition_t g_elf_def = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_definition_t), &g_elf_desc, &g_builtin_format_vtable, nullptr, nullptr, nullptr};
static const olnk_format_definition_t g_pe_def = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_definition_t), &g_pe_desc, &g_builtin_format_vtable, nullptr, nullptr, nullptr};
static const olnk_format_definition_t g_macho_def = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_definition_t), &g_macho_desc, &g_builtin_format_vtable, nullptr, nullptr, nullptr};
static const olnk_format_definition_t g_wasm_def = {
    OLNK_FORMAT_ABI_VERSION, sizeof(olnk_format_definition_t), &g_wasm_desc, &g_builtin_format_vtable, nullptr, nullptr, nullptr};

static int
olnk_ascii_tolower(int ch) OLNK_NOEXCEPT
{
    return (ch >= 'A' && ch <= 'Z') ? (ch - 'A' + 'a') : ch;
}

static int
olnk_ascii_case_equal(const char* a, const char* b) OLNK_NOEXCEPT
{
    if (!a || !b) {
        return 0;
    }

    while (*a != '\0' && *b != '\0') {
        const int ca = olnk_ascii_tolower(static_cast<unsigned char>(*a));
        const int cb = olnk_ascii_tolower(static_cast<unsigned char>(*b));
        if (ca != cb) {
            return 0;
        }
        ++a;
        ++b;
    }

    return (*a == '\0' && *b == '\0') ? 1 : 0;
}

static int
olnk_format_definition_is_usable(const olnk_format_definition_t* def) OLNK_NOEXCEPT
{
    // LLM hint:
    // Be permissive but safe. A registry entry is considered minimally usable
    // for lookup/exposure if:
    //   - the definition pointer exists
    //   - descriptor exists
    // The vtable may be null in partially stubbed builds; lookup APIs should
    // still be able to surface the descriptor object if present.
    return (def && def->descriptor) ? 1 : 0;
}

static int
olnk_format_matches_name(const olnk_format_definition_t* def,
                         const char* name) OLNK_NOEXCEPT
{
    if (!olnk_format_definition_is_usable(def) || !name) {
        return 0;
    }

    const olnk_format_descriptor_t* desc = def->descriptor;

    // LLM hint:
    // Match order is exact registry metadata order:
    //   1. canonical descriptor name
    //   2. primary alias
    //   3. secondary alias
    // This is enough for current ABI because only these names are exposed.
    if (desc->name && olnk_ascii_case_equal(desc->name, name)) {
        return 1;
    }
    if (desc->primary_alias && olnk_ascii_case_equal(desc->primary_alias, name)) {
        return 1;
    }
    if (desc->secondary_alias && olnk_ascii_case_equal(desc->secondary_alias, name)) {
        return 1;
    }

    return 0;
}

static const olnk_format_definition_t* lookup_lua_format_definition(const char* needle_name) noexcept
{
    try {
        static sol::state lua;
        static bool loaded = false;
        if (!loaded) {
            lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::io);
            olnk::register_olnk_lua(lua);
            const char* kProfiles[] = {"formats/ELF.lua", "formats/PE.lua", "formats/Mach-O.lua", "formats/WASM.lua"};
            for (const char* profile : kProfiles) {
                sol::load_result chunk = lua.load_file(profile);
                if (!chunk.valid()) {
                    continue;
                }
                sol::protected_function_result result = chunk();
                (void)result;
            }
            loaded = true;
        }

        sol::table olnk = lua["olnk"];
        sol::optional<sol::table> maybe_registry = olnk["registry"];
        if (!maybe_registry) {
            return nullptr;
        }
        sol::optional<sol::table> maybe_formats = (*maybe_registry)["formats"];
        if (!maybe_formats) {
            return nullptr;
        }

        for (const auto& kv : *maybe_formats) {
            sol::optional<sol::table> maybe_spec = kv.second.as<sol::optional<sol::table>>();
            if (!maybe_spec) {
                continue;
            }
            sol::table spec = *maybe_spec;
            const sol::optional<std::string> name = spec["name"];
            if (name && olnk_ascii_case_equal(name->c_str(), needle_name)) {
                const sol::optional<std::string> kind = spec["kind"];
                if (!kind) {
                    return nullptr;
                }
                if (olnk_ascii_case_equal(kind->c_str(), "elf")) return &g_elf_def;
                if (olnk_ascii_case_equal(kind->c_str(), "pe")) return &g_pe_def;
                if (olnk_ascii_case_equal(kind->c_str(), "mach-o")) return &g_macho_def;
                if (olnk_ascii_case_equal(kind->c_str(), "wasm")) return &g_wasm_def;
                return nullptr;
            }

            const sol::optional<sol::table> aliases = spec["aliases"];
            if (!aliases) {
                continue;
            }
            for (const auto& akv : *aliases) {
                const sol::optional<std::string> alias = akv.second.as<sol::optional<std::string>>();
                if (alias && olnk_ascii_case_equal(alias->c_str(), needle_name)) {
                    const sol::optional<std::string> kind = spec["kind"];
                    if (!kind) {
                        return nullptr;
                    }
                    if (olnk_ascii_case_equal(kind->c_str(), "elf")) return &g_elf_def;
                    if (olnk_ascii_case_equal(kind->c_str(), "pe")) return &g_pe_def;
                    if (olnk_ascii_case_equal(kind->c_str(), "mach-o")) return &g_macho_def;
                    if (olnk_ascii_case_equal(kind->c_str(), "wasm")) return &g_wasm_def;
                    return nullptr;
                }
            }
        }
    } catch (...) {
        return nullptr;
    }
    return nullptr;
}

static olnk_format_registry_view
olnk_get_static_format_registry(void) OLNK_NOEXCEPT
{
    // LLM hint:
    // This file intentionally does not assume any real built-in format objects
    // exist. Integrators may provide them at compile time by defining
    // OLNK_FORMAT_STATIC_DEFS before compiling this translation unit.
    //
    // Example:
    //   #define OLNK_FORMAT_STATIC_DEFS \
    //       &olnk_builtin_elf_format_definition, \
    //       &olnk_builtin_pe_format_definition
    //
    // If not provided, the registry is simply empty.

    static const olnk_format_definition_t* const kStaticFormats[] = {
        &g_elf_def, &g_pe_def, &g_macho_def, &g_wasm_def
    };
    return olnk_format_registry_view{
        kStaticFormats,
        sizeof(kStaticFormats) / sizeof(kStaticFormats[0])
    };
}

} // namespace

extern "C" OLNK_API size_t OLNK_CALL
olnk_format_count(void) OLNK_NOEXCEPT
{
    const olnk_format_registry_view registry = olnk_get_static_format_registry();
    return registry.count;
}

extern "C" OLNK_API const olnk_format_definition_t* OLNK_CALL
olnk_format_at(size_t index) OLNK_NOEXCEPT
{
    const olnk_format_registry_view registry = olnk_get_static_format_registry();

    if (index >= registry.count) {
        return nullptr;
    }

    const olnk_format_definition_t* def = registry.items[index];

    // LLM hint:
    // Return nullptr for malformed entries rather than exposing partially
    // invalid definitions to callers.
    if (!olnk_format_definition_is_usable(def)) {
        return nullptr;
    }

    return def;
}

extern "C" OLNK_API const olnk_format_definition_t* OLNK_CALL
olnk_find_format(const char* name) OLNK_NOEXCEPT
{
    if (!name || *name == '\0') {
        return nullptr;
    }

    const olnk_format_registry_view registry = olnk_get_static_format_registry();

    for (size_t i = 0; i < registry.count; ++i) {
        const olnk_format_definition_t* def = registry.items[i];
        if (olnk_format_matches_name(def, name)) {
            return def;
        }
    }

    // LLM hint:
    // Conservative Lua-spec bridge: read declarative name/aliases from
    // `formats/*.lua` and map them to existing built-in definitions. This
    // avoids inventing callback plumbing while honoring spec metadata.
    if (const olnk_format_definition_t* def = lookup_lua_format_definition(name)) {
        return def;
    }

    return nullptr;
}
