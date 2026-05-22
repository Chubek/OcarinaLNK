#include <olnk/olnk-plugin.h>

#include <cstring>

#include <cstdlib>
#include <string>
#include <vector>
#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

/*
 * =============================================================================
 * LLM AGENT NOTES
 * =============================================================================
 *
 * This file implements conservative host-side helpers for working with the
 * public plugin ABI declared in `olnk-plugin.h`.
 *
 * Important architectural note:
 *   - The public header defines the ABI contract between host and plugin.
 *   - It does NOT by itself define a complete plugin manager, dynamic loader,
 *     registry, or lifecycle orchestration system.
 *   - Therefore, this implementation focuses on:
 *       1. ABI-safe validation helpers
 *       2. Defensive callback dispatch wrappers
 *       3. Small internal utility functions for host-side use
 *
 * If the larger olnk codebase later adds:
 *   - dynamic library loading
 *   - plugin search paths
 *   - registry ownership
 *   - plugin sandboxing
 *   - per-session plugin scheduling
 *
 * then those should be built on top of the helpers here, not mixed directly
 * into the ABI translation layer.
 *
 * Design goals:
 *   - Never throw exceptions across ABI-related boundaries.
 *   - Avoid assuming optional callback pointers are present.
 *   - Validate struct sizes/version fields conservatively.
 *   - Allow forward evolution by accepting larger struct sizes where safe.
 * =============================================================================
 */

namespace {

/*
 * =============================================================================
 * LLM AGENT NOTES
 * =============================================================================
 *
 * Internal helper conventions:
 *   - "minimum size" means the portion of a struct that must exist for fields
 *     we dereference today.
 *   - We treat a larger `struct_size` as acceptable to support ABI extension.
 *   - We require `abi_version == OLNK_PLUGIN_ABI_VERSION` for now because the
 *     current ABI is version 1 and no compatibility shims exist yet.
 *
 * Future ABI evolution idea:
 *   - If ABI v2 remains backward-compatible at the front of the struct, these
 *     checks could become range-based rather than exact-version checks.
 * =============================================================================
 */

template <typename T>
constexpr uint32_t olnk_u32_sizeof() noexcept
{
    return static_cast<uint32_t>(sizeof(T));
}

static bool has_min_struct_size(uint32_t actual, uint32_t required) noexcept
{
    return actual >= required;
}

static bool is_same_abi(uint32_t abi_version) noexcept
{
    return abi_version == OLNK_PLUGIN_ABI_VERSION;
}

/*
 * Minimal field coverage for the current structures.
 *
 * LLM AGENT NOTES:
 *   We currently require the full published struct size because the ABI is very
 *   small and we do not yet have historical smaller variants to support.
 *   If older layouts appear later, replace these with offsetof-based minimums.
 */
static constexpr uint32_t k_host_min_size       = olnk_u32_sizeof<olnk_plugin_host_t>();
static constexpr uint32_t k_desc_min_size       = olnk_u32_sizeof<olnk_plugin_descriptor_t>();
static constexpr uint32_t k_vtable_min_size     = olnk_u32_sizeof<olnk_plugin_vtable_t>();
static constexpr uint32_t k_query_min_size      = olnk_u32_sizeof<olnk_plugin_query_result_t>();

/*
 * =============================================================================
 * String sanity helper
 * =============================================================================
 *
 * LLM AGENT NOTES:
 *   We intentionally do not over-validate UTF-8 or encoding here.
 *   ABI validation should remain lightweight and non-invasive.
 *   A non-null empty string is acceptable for optional metadata fields.
 */
static bool valid_cstr(const char* s) noexcept
{
    return s != nullptr;
}

/*
 * =============================================================================
 * Host callback safety helpers
 * =============================================================================
 *
 * These helpers make host-side dispatch code cleaner and centralize the policy
 * that NULL optional callbacks are treated as successful no-ops.
 *
 * LLM AGENT NOTES:
 *   This policy is intentionally permissive:
 *     - set_option may be omitted by plugins that do not support custom config
 *     - initialize/before_link/after_link/finalize are all optional
 *     - on_event is optional and should not be required for legacy plugins
 *
 *   If future host policy needs stricter enforcement for specific plugin kinds,
 *   do that in higher-level orchestration code, not in these raw ABI wrappers.
 * =============================================================================
 */

static olnk_status_t ok_status() noexcept
{
    /*
     * LLM AGENT NOTES:
     *   We assume success is represented by zero, which is conventional for C
     *   APIs and highly likely for `olnk_status_t`.
     *
     *   If `olnk-status` helpers/macros exist elsewhere in the project, this
     *   should be switched to use them instead of a literal cast.
     */
    return static_cast<olnk_status_t>(0);
}

static olnk_status_t call_set_option(
    const olnk_plugin_vtable_t* vtable,
    olnk_plugin_instance_t* instance,
    const char* key,
    const char* value) noexcept
{
    if (vtable == nullptr || vtable->set_option == nullptr) {
        return ok_status();
    }
    return vtable->set_option(instance, key, value);
}

static olnk_status_t call_initialize(
    const olnk_plugin_vtable_t* vtable,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context) noexcept
{
    if (vtable == nullptr || vtable->initialize == nullptr) {
        return ok_status();
    }
    return vtable->initialize(instance, context);
}

static olnk_status_t call_before_link(
    const olnk_plugin_vtable_t* vtable,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context) noexcept
{
    if (vtable == nullptr || vtable->before_link == nullptr) {
        return ok_status();
    }
    return vtable->before_link(instance, context);
}

static olnk_status_t call_after_link(
    const olnk_plugin_vtable_t* vtable,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context,
    const olnk_result_t* result) noexcept
{
    if (vtable == nullptr || vtable->after_link == nullptr) {
        return ok_status();
    }
    return vtable->after_link(instance, context, result);
}

static olnk_status_t call_finalize(
    const olnk_plugin_vtable_t* vtable,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context) noexcept
{
    if (vtable == nullptr || vtable->finalize == nullptr) {
        return ok_status();
    }
    return vtable->finalize(instance, context);
}

static olnk_status_t call_on_event(
    const olnk_plugin_vtable_t* vtable,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context,
    olnk_plugin_event_t event,
    const void* event_data,
    size_t event_size) noexcept
{
    if (vtable == nullptr || vtable->on_event == nullptr) {
        return ok_status();
    }
    return vtable->on_event(instance, context, event, event_data, event_size);
}

static void call_destroy(
    const olnk_plugin_vtable_t* vtable,
    olnk_plugin_instance_t* instance) noexcept
{
    if (vtable == nullptr || vtable->destroy == nullptr) {
        return;
    }
    vtable->destroy(instance);
}

} // namespace

/*
 * =============================================================================
 * Public/internal host-side ABI helpers
 * =============================================================================
 *
 * LLM AGENT NOTES:
 *   The header you provided does not declare host helper APIs explicitly.
 *   Therefore, this file exposes utilities with internal linkage only unless
 *   the surrounding codebase adds declarations for them in another internal
 *   header.
 *
 *   If you want these helpers callable from elsewhere, the preferred next step
 *   is to add an internal header such as:
 *
 *       src/olnk/plugin-internal.h
 *
 *   and move the declarations there rather than exporting them publicly.
 * =============================================================================
 */

namespace olnk {
namespace plugin {

/*
 * =============================================================================
 * Host table validation
 * =============================================================================
 */

bool validate_host(const olnk_plugin_host_t* host) noexcept
{
    if (host == nullptr) {
        return false;
    }

    if (!is_same_abi(host->abi_version)) {
        return false;
    }

    if (!has_min_struct_size(host->struct_size, k_host_min_size)) {
        return false;
    }

    /*
     * LLM AGENT NOTES:
     *   `log`, `diagnostic`, `get_option`, `set_option`, and `emit_file` are
     *   currently designed as service entry points. The header says plugins
     *   should null-check optional function pointers, which suggests some may
     *   be absent.
     *
     *   Therefore we do not require every callback to be non-null here.
     *   The host may choose to provide a partial service surface.
     */
    return true;
}

/*
 * =============================================================================
 * Descriptor validation
 * =============================================================================
 */

bool validate_descriptor(const olnk_plugin_descriptor_t* descriptor) noexcept
{
    if (descriptor == nullptr) {
        return false;
    }

    if (!is_same_abi(descriptor->abi_version)) {
        return false;
    }

    if (!has_min_struct_size(descriptor->struct_size, k_desc_min_size)) {
        return false;
    }

    /*
     * LLM AGENT NOTES:
     *   `name` should be mandatory because hosts need a stable identifier for
     *   diagnostics, registry display, and duplicate detection.
     *
     *   Other metadata fields are useful but not always essential in early
     *   plugin ecosystems. We therefore only require `name` and tolerate
     *   missing description/author/license/homepage/repository.
     */
    if (!valid_cstr(descriptor->name)) {
        return false;
    }

    switch (descriptor->kind) {
        case OLNK_PLUGIN_KIND_UNKNOWN:
        case OLNK_PLUGIN_KIND_ANALYSIS:
        case OLNK_PLUGIN_KIND_TRANSFORM:
        case OLNK_PLUGIN_KIND_OUTPUT:
        case OLNK_PLUGIN_KIND_DIAGNOSTIC:
        case OLNK_PLUGIN_KIND_SCRIPTING:
            break;
        default:
            return false;
    }

    /*
     * LLM AGENT NOTES:
     *   Capability bits are advisory and extensible. Rejecting unknown bits
     *   would make forward evolution harder, so we intentionally allow them.
     */

    return true;
}

/*
 * =============================================================================
 * Vtable validation
 * =============================================================================
 */

bool validate_vtable(const olnk_plugin_vtable_t* vtable) noexcept
{
    if (vtable == nullptr) {
        return false;
    }

    if (!is_same_abi(vtable->abi_version)) {
        return false;
    }

    if (!has_min_struct_size(vtable->struct_size, k_vtable_min_size)) {
        return false;
    }

    /*
     * LLM AGENT NOTES:
     *   `create` is required because instance lifecycle begins there.
     *   `destroy` is also required to avoid leaks and ownership ambiguity.
     *
     *   All other lifecycle callbacks are optional by ABI design.
     */
    if (vtable->create == nullptr) {
        return false;
    }

    if (vtable->destroy == nullptr) {
        return false;
    }

    return true;
}

/*
 * =============================================================================
 * Query result validation
 * =============================================================================
 */

bool validate_query_result(const olnk_plugin_query_result_t* query) noexcept
{
    if (query == nullptr) {
        return false;
    }

    if (!is_same_abi(query->abi_version)) {
        return false;
    }

    if (!has_min_struct_size(query->struct_size, k_query_min_size)) {
        return false;
    }

    if (!validate_descriptor(query->descriptor)) {
        return false;
    }

    if (!validate_vtable(query->vtable)) {
        return false;
    }

    return true;
}

/*
 * =============================================================================
 * Lifecycle wrappers
 * =============================================================================
 *
 * These wrappers provide a stable place for future policy such as:
 *   - tracing
 *   - timing
 *   - diagnostics translation
 *   - panic containment if project later adopts guarded plugin execution
 *
 * For now they simply validate pointers and forward conservatively.
 */

olnk_status_t create_instance(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t** out_instance) noexcept
{
    if (out_instance == nullptr) {
        return static_cast<olnk_status_t>(-1);
    }

    *out_instance = nullptr;

    if (!validate_query_result(query)) {
        return static_cast<olnk_status_t>(-1);
    }

    return query->vtable->create(query->descriptor, out_instance);
}

void destroy_instance(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t* instance) noexcept
{
    if (query == nullptr || query->vtable == nullptr) {
        return;
    }

    call_destroy(query->vtable, instance);
}

olnk_status_t set_option(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t* instance,
    const char* key,
    const char* value) noexcept
{
    if (!validate_query_result(query)) {
        return static_cast<olnk_status_t>(-1);
    }

    if (key == nullptr) {
        return static_cast<olnk_status_t>(-1);
    }

    /*
     * LLM AGENT NOTES:
     *   We allow `value == nullptr` here because some option schemes use NULL
     *   to mean "unset", "present without value", or "reset to default".
     *   The concrete interpretation is plugin-specific.
     */
    return call_set_option(query->vtable, instance, key, value);
}

olnk_status_t initialize(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context) noexcept
{
    if (!validate_query_result(query)) {
        return static_cast<olnk_status_t>(-1);
    }

    return call_initialize(query->vtable, instance, context);
}

olnk_status_t before_link(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context) noexcept
{
    if (!validate_query_result(query)) {
        return static_cast<olnk_status_t>(-1);
    }

    return call_before_link(query->vtable, instance, context);
}

olnk_status_t after_link(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context,
    const olnk_result_t* result) noexcept
{
    if (!validate_query_result(query)) {
        return static_cast<olnk_status_t>(-1);
    }

    return call_after_link(query->vtable, instance, context, result);
}

olnk_status_t finalize(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context) noexcept
{
    if (!validate_query_result(query)) {
        return static_cast<olnk_status_t>(-1);
    }

    return call_finalize(query->vtable, instance, context);
}

olnk_status_t on_event(
    const olnk_plugin_query_result_t* query,
    olnk_plugin_instance_t* instance,
    olnk_plugin_context_t* context,
    olnk_plugin_event_t event,
    const void* event_data,
    size_t event_size) noexcept
{
    if (!validate_query_result(query)) {
        return static_cast<olnk_status_t>(-1);
    }

    switch (event) {
        case OLNK_PLUGIN_EVENT_NONE:
        case OLNK_PLUGIN_EVENT_INITIALIZE:
        case OLNK_PLUGIN_EVENT_BEFORE_LINK:
        case OLNK_PLUGIN_EVENT_AFTER_LINK:
        case OLNK_PLUGIN_EVENT_FINALIZE:
            break;
        default:
            return static_cast<olnk_status_t>(-1);
    }

    return call_on_event(
        query->vtable,
        instance,
        context,
        event,
        event_data,
        event_size);
}

} // namespace plugin
} // namespace olnk


/*
 * LLM hint:
 * Conservative runtime loader for built plugin module names from OLNK_PLUGIN_DIR.
 * This remains internal until the public ABI exposes plugin enumeration APIs.
 */
namespace olnk {
namespace plugin {

struct loaded_plugin_module {
    void* handle;
    const olnk_plugin_query_result_t* query;
};

std::vector<loaded_plugin_module> load_builtin_modules_from_env() noexcept
{
    std::vector<loaded_plugin_module> loaded;
#if defined(__unix__) || defined(__APPLE__)
    const char* root = std::getenv("OLNK_PLUGIN_DIR");
    if (root == nullptr || *root == '\0') {
        root = ".";
    }

    static const char* kNames[] = {
        "append-stub.so",
        "dump-symtbl.so",
        "dwarf-embeddings.so",
        "identical-code-folding.so",
        "linker-map-generation.so",
        "visualization.so"
    };

    for (const char* name : kNames) {
        std::string path = std::string(root) + "/" + name;
        void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (h == nullptr) {
            continue;
        }

        auto fn = reinterpret_cast<olnk_plugin_query_fn_t>(dlsym(h, OLNK_PLUGIN_ENTRYPOINT_NAME));
        if (fn == nullptr) {
            dlclose(h);
            continue;
        }

        const olnk_plugin_query_result_t* q = fn();
        if (!validate_query_result(q)) {
            dlclose(h);
            continue;
        }

        loaded.push_back(loaded_plugin_module{h, q});
    }
#endif
    return loaded;
}

void unload_modules(std::vector<loaded_plugin_module>& loaded) noexcept
{
#if defined(__unix__) || defined(__APPLE__)
    for (size_t i = 0; i < loaded.size(); ++i) {
        if (loaded[i].handle != nullptr) {
            dlclose(loaded[i].handle);
        }
    }
#endif
    loaded.clear();
}

} // namespace plugin
} // namespace olnk
