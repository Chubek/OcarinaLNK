#include <olnk/olnk-machine.h>

#include <sol/sol.hpp>

#include <cstring>
#include <fstream>
#include <string>

namespace olnk {
void register_olnk_lua(sol::state& lua);
}

/*
 * =============================================================================
 * LLM AGENT NOTES / IMPLEMENTATION GUIDANCE
 * =============================================================================
 *
 * This file provides the built-in machine registry for olnk.
 *
 * Design goals of this implementation:
 *   - Be ABI-conservative and host-owned.
 *   - Expose a stable set of built-in machine definitions.
 *   - Avoid heap allocation and complicated global initialization.
 *   - Make lookup deterministic and easy to extend.
 *   - Keep all returned pointers valid for the entire process lifetime.
 *
 * Important current scope:
 *   - This is a registry/metadata implementation, not a full backend.
 *   - Built-in machine definitions are "static descriptors + simple vtables".
 *   - Validation is intentionally lightweight and mostly returns success.
 *   - Machine instances are not heap-backed yet; `create()` returns a shared
 *     singleton-ish opaque instance per machine definition.
 *
 * Future extension ideas:
 *   1. Replace the trivial instance model with per-instance option storage.
 *   2. Add config-aware validation once config getters or internal accessors
 *      are available from the core API layer.
 *   3. Support external machine modules/plugins merged into this registry.
 *   4. Add alias canonicalization and target-triple parsing helpers.
 *   5. Split "machine ISA" from ABI environment if required later.
 *   6. Introduce internal registration APIs in a private header if dynamic
 *      registry extension becomes necessary.
 *
 * ABI safety notes:
 *   - All public structs with size/version fields are validated conservatively
 *     before use.
 *   - Descriptor/property strings are static lifetime.
 *   - `olnk_machine_at()` and lookup functions return host-owned pointers.
 * =============================================================================
 */

// LLM hint: define the opaque ABI instance in global scope (not inside an
// anonymous namespace) so it satisfies the forward declaration from the public header.
struct olnk_machine_instance {
    const olnk_machine_definition_t* definition;
};

namespace {

/*
 * =============================================================================
 * ABI / validation helpers
 * =============================================================================
 */

template <typename T>
static bool struct_header_is_compatible(const T* p, uint32_t expected_abi) noexcept
{
    return p != nullptr &&
           p->abi_version == expected_abi &&
           p->struct_size >= sizeof(T);
}

static bool cstr_equals(const char* a, const char* b) noexcept
{
    if (a == nullptr || b == nullptr) {
        return false;
    }
    return std::strcmp(a, b) == 0;
}

static bool cstr_iequals_ascii(const char* a, const char* b) noexcept
{
    if (a == nullptr || b == nullptr) {
        return false;
    }

    while (*a != '\0' && *b != '\0') {
        char ca = *a;
        char cb = *b;

        if (ca >= 'A' && ca <= 'Z') {
            ca = static_cast<char>(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = static_cast<char>(cb - 'A' + 'a');
        }

        if (ca != cb) {
            return false;
        }

        ++a;
        ++b;
    }

    return *a == '\0' && *b == '\0';
}

static bool lua_machine_spec_matches_name(const char* name, olnk_machine_kind_t* out_kind) noexcept
{
    if (out_kind == nullptr) {
        return false;
    }
    *out_kind = OLNK_MACHINE_KIND_UNKNOWN;

    try {
        static sol::state lua;
        static bool loaded = false;
        if (!loaded) {
            lua.open_libraries(sol::lib::base, sol::lib::package, sol::lib::table, sol::lib::string, sol::lib::math, sol::lib::io);
            olnk::register_olnk_lua(lua);
            const char* kProfiles[] = {"machines/WASM.lua", "machines/x86-64.lua", "machines/Aarch64.lua"};
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
            return false;
        }
        sol::optional<sol::table> maybe_machines = (*maybe_registry)["machines"];
        if (!maybe_machines) {
            return false;
        }

        for (const auto& kv : *maybe_machines) {
            sol::optional<sol::table> maybe_spec = kv.second.as<sol::optional<sol::table>>();
            if (!maybe_spec) {
                continue;
            }
            sol::table spec = *maybe_spec;

            bool matched = false;
            const sol::optional<std::string> spec_name = spec["name"];
            if (spec_name && cstr_iequals_ascii(spec_name->c_str(), name)) {
                matched = true;
            }
            const sol::optional<std::string> spec_arch = spec["arch"];
            if (!matched && spec_arch && cstr_iequals_ascii(spec_arch->c_str(), name)) {
                matched = true;
            }
            if (!matched) {
                const sol::optional<sol::table> aliases = spec["aliases"];
                if (aliases) {
                    for (const auto& akv : *aliases) {
                        const sol::optional<std::string> alias = akv.second.as<sol::optional<std::string>>();
                        if (alias && cstr_iequals_ascii(alias->c_str(), name)) {
                            matched = true;
                            break;
                        }
                    }
                }
            }
            if (!matched) {
                continue;
            }

            if (spec_arch && cstr_iequals_ascii(spec_arch->c_str(), "wasm32")) {
                *out_kind = OLNK_MACHINE_KIND_WASM32;
                return true;
            }
            if (spec_arch && cstr_iequals_ascii(spec_arch->c_str(), "x86_64")) {
                *out_kind = OLNK_MACHINE_KIND_X86_64;
                return true;
            }
            if (spec_arch && cstr_iequals_ascii(spec_arch->c_str(), "aarch64")) {
                *out_kind = OLNK_MACHINE_KIND_AARCH64;
                return true;
            }
            return false;
        }
    } catch (...) {
        return false;
    }

    return false;
}

static const char* fallback_empty(const char* s) noexcept
{
    return s != nullptr ? s : "";
}

/*
 * =============================================================================
 * Generic built-in machine callback implementations
 * =============================================================================
 */

static olnk_status_t OLNK_CALL builtin_machine_create(
    const olnk_machine_descriptor_t* descriptor,
    olnk_machine_instance_t** out_instance) OLNK_NOEXCEPT;

static void OLNK_CALL builtin_machine_destroy(
    olnk_machine_instance_t* instance) OLNK_NOEXCEPT;

static olnk_status_t OLNK_CALL builtin_machine_set_option(
    olnk_machine_instance_t* instance,
    const char* key,
    const char* value) OLNK_NOEXCEPT;

static olnk_status_t OLNK_CALL builtin_machine_get_properties(
    olnk_machine_instance_t* instance,
    olnk_machine_properties_t* out_properties) OLNK_NOEXCEPT;

static olnk_status_t OLNK_CALL builtin_machine_validate_config(
    olnk_machine_instance_t* instance,
    olnk_machine_context_t* context) OLNK_NOEXCEPT;

static const char* OLNK_CALL builtin_machine_canonical_name(
    const olnk_machine_instance_t* instance) OLNK_NOEXCEPT;

static const char* OLNK_CALL builtin_machine_canonical_triple(
    const olnk_machine_instance_t* instance) OLNK_NOEXCEPT;

static const char* OLNK_CALL builtin_machine_describe(
    const olnk_machine_instance_t* instance) OLNK_NOEXCEPT;

/*
 * =============================================================================
 * Built-in machine metadata table
 * =============================================================================
 *
 * LLM AGENT NOTES:
 *   Keep entries static and constexpr-like in spirit.
 *   If adding a new built-in machine:
 *     1. add descriptor
 *     2. add properties
 *     3. add instance
 *     4. add definition
 *     5. append to registry array
 *
 *   The current table intentionally includes many common targets but assigns
 *   conservative defaults for page size / branch range where the ABI is not
 *   universal. Those values are advisory only.
 */

struct BuiltinMachineRecord {
    olnk_machine_descriptor_t descriptor;
    olnk_machine_properties_t properties;
    olnk_machine_instance_t instance{};
    olnk_machine_definition_t definition;
};

#define OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(kind_value, caps_value, name_value, desc_value, alias1, alias2, triple_value) \
    {                                                                                                                        \
        OLNK_MACHINE_ABI_VERSION,                                                                                            \
        sizeof(olnk_machine_descriptor_t),                                                                                   \
        name_value,                                                                                                          \
        desc_value,                                                                                                          \
        "olnk",                                                                                                              \
        "MIT",                                                                                                               \
        0u, 1u, 0u,                                                                                                          \
        kind_value,                                                                                                          \
        caps_value,                                                                                                          \
        0u,                                                                                                                  \
        0xFFFFFFFFu,                                                                                                         \
        alias1,                                                                                                              \
        alias2,                                                                                                              \
        triple_value,                                                                                                        \
        nullptr,                                                                                                             \
        nullptr                                                                                                              \
    }

#define OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(kind_value, caps_value, endian_value, addr_class_value, ptr_size_value, addr_size_value, instr_align_value, stack_align_value, func_align_value, image_base_value, page_size_value, branch_range_value, code_model_value, canon_name_value, triple_value) \
    {                                                                                                                                                                                                                       \
        OLNK_MACHINE_ABI_VERSION,                                                                                                                                                                                           \
        sizeof(olnk_machine_properties_t),                                                                                                                                                                                  \
        kind_value,                                                                                                                                                                                                         \
        caps_value,                                                                                                                                                                                                         \
        endian_value,                                                                                                                                                                                                       \
        addr_class_value,                                                                                                                                                                                                   \
        ptr_size_value,                                                                                                                                                                                                     \
        addr_size_value,                                                                                                                                                                                                    \
        instr_align_value,                                                                                                                                                                                                  \
        stack_align_value,                                                                                                                                                                                                  \
        func_align_value,                                                                                                                                                                                                   \
        image_base_value,                                                                                                                                                                                                   \
        page_size_value,                                                                                                                                                                                                    \
        branch_range_value,                                                                                                                                                                                                 \
        code_model_value,                                                                                                                                                                                                   \
        canon_name_value,                                                                                                                                                                                                   \
        triple_value,                                                                                                                                                                                                       \
        nullptr,                                                                                                                                                                                                            \
        nullptr                                                                                                                                                                                                             \
    }

static constexpr uint32_t k_caps_native_common =
    OLNK_MACHINE_CAPABILITY_EXECUTABLE |
    OLNK_MACHINE_CAPABILITY_SHARED_LIBRARY |
    OLNK_MACHINE_CAPABILITY_RELOCATABLE_OUTPUT |
    OLNK_MACHINE_CAPABILITY_TLS |
    OLNK_MACHINE_CAPABILITY_PIC |
    OLNK_MACHINE_CAPABILITY_GOT |
    OLNK_MACHINE_CAPABILITY_PLT;

static constexpr uint32_t k_caps_native_relax =
    k_caps_native_common |
    OLNK_MACHINE_CAPABILITY_RELAXATION;

static constexpr uint32_t k_caps_native_branch =
    k_caps_native_relax |
    OLNK_MACHINE_CAPABILITY_BRANCH_VENEERS;

static constexpr uint32_t k_caps_x86_64 =
    k_caps_native_common |
    OLNK_MACHINE_CAPABILITY_ATOMICS |
    OLNK_MACHINE_CAPABILITY_SIMD |
    OLNK_MACHINE_CAPABILITY_UNALIGNED_ACCESS;

static constexpr uint32_t k_caps_i386 =
    k_caps_native_common |
    OLNK_MACHINE_CAPABILITY_ATOMICS |
    OLNK_MACHINE_CAPABILITY_SIMD |
    OLNK_MACHINE_CAPABILITY_UNALIGNED_ACCESS;

static constexpr uint32_t k_caps_aarch64 =
    k_caps_native_branch |
    OLNK_MACHINE_CAPABILITY_ATOMICS |
    OLNK_MACHINE_CAPABILITY_SIMD;

static constexpr uint32_t k_caps_arm =
    k_caps_native_branch |
    OLNK_MACHINE_CAPABILITY_ATOMICS;

static constexpr uint32_t k_caps_riscv =
    k_caps_native_relax |
    OLNK_MACHINE_CAPABILITY_ATOMICS;

static constexpr uint32_t k_caps_ppc =
    k_caps_native_common |
    OLNK_MACHINE_CAPABILITY_ATOMICS;

static constexpr uint32_t k_caps_mips =
    k_caps_native_common;

static constexpr uint32_t k_caps_sparc =
    k_caps_native_common;

static constexpr uint32_t k_caps_systemz =
    k_caps_native_common |
    OLNK_MACHINE_CAPABILITY_ATOMICS;

static constexpr uint32_t k_caps_loongarch =
    k_caps_native_relax |
    OLNK_MACHINE_CAPABILITY_ATOMICS;

static constexpr uint32_t k_caps_wasm =
    OLNK_MACHINE_CAPABILITY_RELOCATABLE_OUTPUT |
    OLNK_MACHINE_CAPABILITY_TLS |
    OLNK_MACHINE_CAPABILITY_PIC |
    OLNK_MACHINE_CAPABILITY_VIRTUAL_ISA;

static constexpr uint32_t k_caps_bpf =
    OLNK_MACHINE_CAPABILITY_RELOCATABLE_OUTPUT |
    OLNK_MACHINE_CAPABILITY_VIRTUAL_ISA;

static BuiltinMachineRecord g_builtin_machines[] = {
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_I386,
            k_caps_i386,
            "i386",
            "Intel 32-bit x86",
            "x86",
            "ia32",
            "i386-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_I386,
            k_caps_i386,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            1u,
            16u,
            16u,
            0x08048000ull,
            4096ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "i386",
            "i386-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_X86_64,
            k_caps_x86_64,
            "x86_64",
            "AMD64 / x86-64",
            "amd64",
            "x64",
            "x86_64-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_X86_64,
            k_caps_x86_64,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            1u,
            16u,
            16u,
            0x400000ull,
            4096ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "x86_64",
            "x86_64-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_ARM,
            k_caps_arm,
            "arm",
            "ARM 32-bit",
            "arm32",
            nullptr,
            "arm-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_ARM,
            k_caps_arm,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            4u,
            8u,
            4u,
            0x00010000ull,
            4096ull,
            33554432ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "arm",
            "arm-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_AARCH64,
            k_caps_aarch64,
            "aarch64",
            "ARM 64-bit",
            "arm64",
            nullptr,
            "aarch64-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_AARCH64,
            k_caps_aarch64,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            4u,
            16u,
            4u,
            0x400000ull,
            4096ull,
            134217728ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "aarch64",
            "aarch64-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_THUMB,
            k_caps_arm,
            "thumb",
            "ARM Thumb instruction set state",
            "thumb2",
            nullptr,
            "thumb-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_THUMB,
            k_caps_arm,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            2u,
            8u,
            2u,
            0x00010000ull,
            4096ull,
            16777216ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "thumb",
            "thumb-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_RISCV32,
            k_caps_riscv,
            "riscv32",
            "RISC-V 32-bit",
            "rv32",
            nullptr,
            "riscv32-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_RISCV32,
            k_caps_riscv,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            2u,
            16u,
            4u,
            0x00010000ull,
            4096ull,
            1048576ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "riscv32",
            "riscv32-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_RISCV64,
            k_caps_riscv,
            "riscv64",
            "RISC-V 64-bit",
            "rv64",
            nullptr,
            "riscv64-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_RISCV64,
            k_caps_riscv,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            2u,
            16u,
            4u,
            0x400000ull,
            4096ull,
            1048576ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "riscv64",
            "riscv64-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_PPC32,
            k_caps_ppc,
            "ppc32",
            "PowerPC 32-bit",
            "powerpc",
            "ppc",
            "powerpc-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_PPC32,
            k_caps_ppc,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            4u,
            16u,
            4u,
            0x00010000ull,
            4096ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "powerpc",
            "powerpc-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_PPC64,
            k_caps_ppc,
            "ppc64",
            "PowerPC 64-bit",
            "powerpc64",
            nullptr,
            "powerpc64-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_PPC64,
            k_caps_ppc,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            4u,
            16u,
            4u,
            0x10000000ull,
            65536ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_MEDIUM,
            "powerpc64",
            "powerpc64-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_MIPS32,
            k_caps_mips,
            "mips32",
            "MIPS 32-bit",
            "mips",
            nullptr,
            "mips-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_MIPS32,
            k_caps_mips,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            4u,
            8u,
            4u,
            0x00400000ull,
            4096ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "mips32",
            "mips-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_MIPS64,
            k_caps_mips,
            "mips64",
            "MIPS 64-bit",
            nullptr,
            nullptr,
            "mips64-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_MIPS64,
            k_caps_mips,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            4u,
            16u,
            4u,
            0x120000000ull,
            16384ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_MEDIUM,
            "mips64",
            "mips64-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_SPARC32,
            k_caps_sparc,
            "sparc32",
            "SPARC 32-bit",
            "sparc",
            nullptr,
            "sparc-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_SPARC32,
            k_caps_sparc,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            4u,
            8u,
            4u,
            0x00010000ull,
            8192ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "sparc",
            "sparc-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_SPARC64,
            k_caps_sparc,
            "sparc64",
            "SPARC 64-bit",
            nullptr,
            nullptr,
            "sparc64-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_SPARC64,
            k_caps_sparc,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            4u,
            16u,
            4u,
            0x100000ull,
            8192ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_MEDIUM,
            "sparc64",
            "sparc64-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_SYSTEMZ,
            k_caps_systemz,
            "systemz",
            "IBM z/Architecture",
            "s390x",
            nullptr,
            "s390x-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_SYSTEMZ,
            k_caps_systemz,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            2u,
            8u,
            2u,
            0x100000ull,
            4096ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_MEDIUM,
            "systemz",
            "s390x-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_LOONGARCH32,
            k_caps_loongarch,
            "loongarch32",
            "LoongArch 32-bit",
            "la32",
            nullptr,
            "loongarch32-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_LOONGARCH32,
            k_caps_loongarch,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            4u,
            16u,
            4u,
            0x00010000ull,
            16384ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "loongarch32",
            "loongarch32-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_LOONGARCH64,
            k_caps_loongarch,
            "loongarch64",
            "LoongArch 64-bit",
            "la64",
            nullptr,
            "loongarch64-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_LOONGARCH64,
            k_caps_loongarch,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            4u,
            16u,
            4u,
            0x400000ull,
            16384ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "loongarch64",
            "loongarch64-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_WASM32,
            k_caps_wasm,
            "wasm32",
            "WebAssembly 32-bit",
            "wasm",
            nullptr,
            "wasm32-unknown-unknown"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_WASM32,
            k_caps_wasm,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_32,
            4u,
            4u,
            1u,
            16u,
            1u,
            0ull,
            65536ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "wasm32",
            "wasm32-unknown-unknown"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_WASM64,
            k_caps_wasm,
            "wasm64",
            "WebAssembly 64-bit",
            nullptr,
            nullptr,
            "wasm64-unknown-unknown"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_WASM64,
            k_caps_wasm,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            1u,
            16u,
            1u,
            0ull,
            65536ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "wasm64",
            "wasm64-unknown-unknown"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_BPFEL,
            k_caps_bpf,
            "bpfel",
            "eBPF little-endian",
            "bpf",
            "ebpf",
            "bpfel-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_BPFEL,
            k_caps_bpf,
            OLNK_MACHINE_ENDIANNESS_LITTLE,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            8u,
            8u,
            8u,
            0ull,
            4096ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "bpfel",
            "bpfel-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
    {
        OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS(
            OLNK_MACHINE_KIND_BPFEB,
            k_caps_bpf,
            "bpfeb",
            "eBPF big-endian",
            nullptr,
            nullptr,
            "bpfeb-unknown-none"),
        OLNK_MACHINE_COMMON_PROPERTIES_FIELDS(
            OLNK_MACHINE_KIND_BPFEB,
            k_caps_bpf,
            OLNK_MACHINE_ENDIANNESS_BIG,
            OLNK_MACHINE_ADDRESS_CLASS_64,
            8u,
            8u,
            8u,
            8u,
            8u,
            0ull,
            4096ull,
            0ull,
            OLNK_MACHINE_CODE_MODEL_SMALL,
            "bpfeb",
            "bpfeb-unknown-none"),
        { nullptr },
        {
            OLNK_MACHINE_ABI_VERSION,
            sizeof(olnk_machine_definition_t),
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr
        }
    },
};

#undef OLNK_MACHINE_COMMON_DESCRIPTOR_FIELDS
#undef OLNK_MACHINE_COMMON_PROPERTIES_FIELDS

static const olnk_machine_vtable_t g_builtin_machine_vtable = {
    OLNK_MACHINE_ABI_VERSION,
    sizeof(olnk_machine_vtable_t),
    &builtin_machine_create,
    &builtin_machine_destroy,
    &builtin_machine_set_option,
    &builtin_machine_get_properties,
    &builtin_machine_validate_config,
    &builtin_machine_canonical_name,
    &builtin_machine_canonical_triple,
    &builtin_machine_describe,
    nullptr,
    nullptr,
    nullptr,
    nullptr
};

static void initialize_builtin_registry() noexcept
{
    /*
     * LLM AGENT NOTES:
     *   We patch the self-referential pointers lazily on first use.
     *   This avoids brittle static initialization ordering across TUs.
     *
     *   Since data is idempotent and deterministic, repeated execution is safe.
     */
    const size_t count = sizeof(g_builtin_machines) / sizeof(g_builtin_machines[0]);
    for (size_t i = 0; i < count; ++i) {
        g_builtin_machines[i].definition.descriptor = &g_builtin_machines[i].descriptor;
        g_builtin_machines[i].definition.vtable = &g_builtin_machine_vtable;
        g_builtin_machines[i].definition.module_data = &g_builtin_machines[i].properties;
        g_builtin_machines[i].instance.definition = &g_builtin_machines[i].definition;
    }
}

static size_t builtin_machine_count() noexcept
{
    return sizeof(g_builtin_machines) / sizeof(g_builtin_machines[0]);
}

static const BuiltinMachineRecord* record_from_definition(
    const olnk_machine_definition_t* definition) noexcept
{
    if (definition == nullptr) {
        return nullptr;
    }

    const size_t count = builtin_machine_count();
    for (size_t i = 0; i < count; ++i) {
        if (&g_builtin_machines[i].definition == definition) {
            return &g_builtin_machines[i];
        }
    }
    return nullptr;
}

static const BuiltinMachineRecord* record_from_instance(
    const olnk_machine_instance_t* instance) noexcept
{
    if (instance == nullptr) {
        return nullptr;
    }

    return record_from_definition(instance->definition);
}

static bool matches_machine_name(
    const BuiltinMachineRecord& rec,
    const char* name) noexcept
{
    return cstr_iequals_ascii(rec.descriptor.name, name) ||
           cstr_iequals_ascii(rec.descriptor.primary_alias, name) ||
           cstr_iequals_ascii(rec.descriptor.secondary_alias, name) ||
           cstr_iequals_ascii(rec.descriptor.canonical_triple, name) ||
           cstr_iequals_ascii(rec.properties.canonical_name, name) ||
           cstr_iequals_ascii(rec.properties.canonical_triple, name);
}

/*
 * =============================================================================
 * Built-in callback implementations
 * =============================================================================
 */

static olnk_status_t OLNK_CALL builtin_machine_create(
    const olnk_machine_descriptor_t* descriptor,
    olnk_machine_instance_t** out_instance) OLNK_NOEXCEPT
{
    if (descriptor == nullptr || out_instance == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    *out_instance = nullptr;

    if (!struct_header_is_compatible(descriptor, OLNK_MACHINE_ABI_VERSION)) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    initialize_builtin_registry();

    const size_t count = builtin_machine_count();
    for (size_t i = 0; i < count; ++i) {
        if (&g_builtin_machines[i].descriptor == descriptor) {
            *out_instance = &g_builtin_machines[i].instance;
            return OLNK_STATUS_OK;
        }
    }

    return OLNK_STATUS_MACHINE_ERROR;
}

static void OLNK_CALL builtin_machine_destroy(
    olnk_machine_instance_t* instance) OLNK_NOEXCEPT
{
    /*
     * LLM AGENT NOTES:
     *   Instances are static and host-owned in this baseline implementation.
     *   Therefore destroy is intentionally a no-op.
     *
     *   If heap-backed instances are introduced later, make sure destroy only
     *   frees instances created by the corresponding machine implementation.
     */
    (void)instance;
}

static olnk_status_t OLNK_CALL builtin_machine_set_option(
    olnk_machine_instance_t* instance,
    const char* key,
    const char* value) OLNK_NOEXCEPT
{
    if (instance == nullptr || key == nullptr || value == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    /*
     * LLM AGENT NOTES:
     *   No per-instance mutable state exists yet, so recognized options are not
     *   persisted. We accept a few generic option names for compatibility and
     *   reject others conservatively as unsupported keys.
     */
    if (cstr_iequals_ascii(key, "cpu") ||
        cstr_iequals_ascii(key, "features") ||
        cstr_iequals_ascii(key, "abi") ||
        cstr_iequals_ascii(key, "code-model")) {
        return OLNK_STATUS_OK;
    }

    (void)instance;
    (void)value;
    return OLNK_STATUS_INVALID_ARGUMENT;
}

static olnk_status_t OLNK_CALL builtin_machine_get_properties(
    olnk_machine_instance_t* instance,
    olnk_machine_properties_t* out_properties) OLNK_NOEXCEPT
{
    if (instance == nullptr || out_properties == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (out_properties->struct_size < sizeof(olnk_machine_properties_t)) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    const BuiltinMachineRecord* rec = record_from_instance(instance);
    if (rec == nullptr) {
        return OLNK_STATUS_MACHINE_ERROR;
    }

    *out_properties = rec->properties;
    return OLNK_STATUS_OK;
}

static void machine_ctx_log(
    olnk_machine_context_t* context,
    olnk_log_level_t level,
    const char* message) noexcept
{
    if (context != nullptr &&
        context->host != nullptr &&
        context->host->log != nullptr &&
        context->host->struct_size >= sizeof(olnk_machine_host_t) &&
        context->host->abi_version == OLNK_MACHINE_ABI_VERSION) {
        context->host->log(context, level, message);
    }
}

static void machine_ctx_diag(
    olnk_machine_context_t* context,
    olnk_diagnostic_severity_t severity,
    const char* message) noexcept
{
    if (context != nullptr &&
        context->host != nullptr &&
        context->host->diagnostic != nullptr &&
        context->host->struct_size >= sizeof(olnk_machine_host_t) &&
        context->host->abi_version == OLNK_MACHINE_ABI_VERSION) {
        context->host->diagnostic(context, severity, message);
    }
}

static olnk_status_t OLNK_CALL builtin_machine_validate_config(
    olnk_machine_instance_t* instance,
    olnk_machine_context_t* context) OLNK_NOEXCEPT
{
    if (instance == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    const BuiltinMachineRecord* rec = record_from_instance(instance);
    if (rec == nullptr) {
        return OLNK_STATUS_MACHINE_ERROR;
    }

    /*
     * LLM AGENT NOTES:
     *   We intentionally keep validation minimal because the public config API
     *   shown so far does not expose typed getters here. Still, this function
     *   is useful as a machine-specific hook point and can emit informational
     *   notes for tracing and diagnostics.
     *
     *   Once internal config accessors are available, add checks such as:
     *     - output kind compatibility
     *     - code model acceptance
     *     - format/machine consistency
     *     - pointer size and ABI environment constraints
     */

    machine_ctx_log(context, OLNK_LOG_DEBUG, fallback_empty(rec->descriptor.description));

    if (olnk_machine_is_virtual_kind(rec->descriptor.kind)) {
        machine_ctx_log(context, OLNK_LOG_DEBUG, "validated virtual machine target");
    } else {
        machine_ctx_log(context, OLNK_LOG_DEBUG, "validated native machine target");
    }

    /*
     * Example conservative diagnostics that are always safe and low-noise.
     */
    if (rec->properties.address_class == OLNK_MACHINE_ADDRESS_CLASS_UNKNOWN) {
        machine_ctx_diag(context, OLNK_DIAGNOSTIC_WARNING,
                         "machine has unknown address class");
    }

    if (rec->properties.endianness == OLNK_MACHINE_ENDIANNESS_UNKNOWN) {
        machine_ctx_diag(context, OLNK_DIAGNOSTIC_WARNING,
                         "machine has unknown endianness");
    }

    return OLNK_STATUS_OK;
}

static const char* OLNK_CALL builtin_machine_canonical_name(
    const olnk_machine_instance_t* instance) OLNK_NOEXCEPT
{
    const BuiltinMachineRecord* rec = record_from_instance(instance);
    if (rec == nullptr) {
        return nullptr;
    }
    return rec->properties.canonical_name;
}

static const char* OLNK_CALL builtin_machine_canonical_triple(
    const olnk_machine_instance_t* instance) OLNK_NOEXCEPT
{
    const BuiltinMachineRecord* rec = record_from_instance(instance);
    if (rec == nullptr) {
        return nullptr;
    }
    return rec->properties.canonical_triple;
}

static const char* OLNK_CALL builtin_machine_describe(
    const olnk_machine_instance_t* instance) OLNK_NOEXCEPT
{
    const BuiltinMachineRecord* rec = record_from_instance(instance);
    if (rec == nullptr) {
        return nullptr;
    }
    return rec->descriptor.description;
}

} // namespace

/*
 * =============================================================================
 * Public registry API
 * =============================================================================
 */

extern "C" OLNK_API size_t OLNK_CALL olnk_machine_count(void) OLNK_NOEXCEPT
{
    initialize_builtin_registry();
    return builtin_machine_count();
}

extern "C" OLNK_API const olnk_machine_definition_t* OLNK_CALL
olnk_machine_at(size_t index) OLNK_NOEXCEPT
{
    initialize_builtin_registry();

    if (index >= builtin_machine_count()) {
        return nullptr;
    }

    return &g_builtin_machines[index].definition;
}

extern "C" OLNK_API const olnk_machine_definition_t* OLNK_CALL
olnk_find_machine(const char* name) OLNK_NOEXCEPT
{
    if (name == nullptr || *name == '\0') {
        return nullptr;
    }

    initialize_builtin_registry();

    const size_t count = builtin_machine_count();
    for (size_t i = 0; i < count; ++i) {
        if (matches_machine_name(g_builtin_machines[i], name)) {
            return &g_builtin_machines[i].definition;
        }
    }

    // LLM hint:
    // Conservative Lua-spec bridge: map declarative machine spec names/arch
    // onto existing built-ins without inventing Lua callback ABI plumbing.
    olnk_machine_kind_t kind = OLNK_MACHINE_KIND_UNKNOWN;
    if (lua_machine_spec_matches_name(name, &kind)) {
        return olnk_find_machine_by_kind(kind);
    }

    /*
     * LLM AGENT NOTES:
     *   Keep alias handling deterministic. If future fuzzy matching or triple
     *   parsing is added, preserve exact-match precedence before heuristics.
     */
    return nullptr;
}

extern "C" OLNK_API const olnk_machine_definition_t* OLNK_CALL
olnk_find_machine_by_kind(olnk_machine_kind_t kind) OLNK_NOEXCEPT
{
    initialize_builtin_registry();

    const size_t count = builtin_machine_count();
    for (size_t i = 0; i < count; ++i) {
        if (g_builtin_machines[i].descriptor.kind == kind) {
            return &g_builtin_machines[i].definition;
        }
    }

    return nullptr;
}

extern "C" OLNK_API const char* OLNK_CALL
olnk_machine_kind_name(olnk_machine_kind_t kind) OLNK_NOEXCEPT
{
    switch (kind) {
        case OLNK_MACHINE_KIND_UNKNOWN: return "unknown";
        case OLNK_MACHINE_KIND_I386: return "i386";
        case OLNK_MACHINE_KIND_X86_64: return "x86_64";
        case OLNK_MACHINE_KIND_ARM: return "arm";
        case OLNK_MACHINE_KIND_AARCH64: return "aarch64";
        case OLNK_MACHINE_KIND_THUMB: return "thumb";
        case OLNK_MACHINE_KIND_RISCV32: return "riscv32";
        case OLNK_MACHINE_KIND_RISCV64: return "riscv64";
        case OLNK_MACHINE_KIND_PPC32: return "ppc32";
        case OLNK_MACHINE_KIND_PPC64: return "ppc64";
        case OLNK_MACHINE_KIND_MIPS32: return "mips32";
        case OLNK_MACHINE_KIND_MIPS64: return "mips64";
        case OLNK_MACHINE_KIND_SPARC32: return "sparc32";
        case OLNK_MACHINE_KIND_SPARC64: return "sparc64";
        case OLNK_MACHINE_KIND_SYSTEMZ: return "systemz";
        case OLNK_MACHINE_KIND_LOONGARCH32: return "loongarch32";
        case OLNK_MACHINE_KIND_LOONGARCH64: return "loongarch64";
        case OLNK_MACHINE_KIND_WASM32: return "wasm32";
        case OLNK_MACHINE_KIND_WASM64: return "wasm64";
        case OLNK_MACHINE_KIND_BPFEL: return "bpfel";
        case OLNK_MACHINE_KIND_BPFEB: return "bpfeb";
        case OLNK_MACHINE_KIND_CUSTOM: return "custom";
        default: return "unrecognized-machine-kind";
    }
}
