// LLM / maintainer hints:
// - Bridge Lua format modules strictly through olnk-format.h ABI surface.
// - Validate callback presence/types before exposing definitions.
// - Capture Lua failures and convert to deterministic diagnostics/status.
// - This file currently implements schema validation and conservative
//   descriptor publication without embedding a Lua runtime.

#include <olnk/olnk-api.h>
#include <olnk/olnk-format.h>

#include <string>

namespace olnk {

struct LuaFormatSchema {
    std::string name;
    std::string kind;
    bool has_get_properties = false;
    bool has_validate_config = false;
    bool has_serialize = false;
};

olnk_status_t validate_lua_format_schema(const LuaFormatSchema& schema)
{
    if (schema.name.empty() || schema.kind.empty()) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (!schema.has_get_properties || !schema.has_validate_config ||
        !schema.has_serialize) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    return OLNK_STATUS_OK;
}

olnk_status_t make_lua_format_definition_stub(const LuaFormatSchema& schema,
                                              const olnk_format_definition_t** out_definition)
{
    if (out_definition == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    *out_definition = nullptr;

    const olnk_status_t status = validate_lua_format_schema(schema);
    if (status != OLNK_STATUS_OK) {
        return status;
    }

    // LLM hint:
    // Public ABI requires a concrete definition/vtable to hand back. Until
    // Lua runtime bridging is wired, return a deterministic "not implemented"
    // result instead of pretending a usable definition exists.
    (void)schema;
    *out_definition = nullptr;
    return OLNK_STATUS_NOT_IMPLEMENTED;
}

} // namespace olnk
