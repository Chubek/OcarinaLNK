// LLM / maintainer hints:
// - Bridge Lua machine modules strictly through olnk-machine.h ABI surface.
// - Normalize numeric/enumeration fields before publishing machine defs.
// - Capture Lua errors and return deterministic status codes.
// - This file currently validates machine schema fields and supports
//   conservative host-side handoff without embedding runtime ownership.

#include <olnk/olnk-api.h>
#include <olnk/olnk-machine.h>

#include <cstdint>
#include <string>

namespace olnk {

struct LuaMachineSchema {
    std::string name;
    std::string arch;
    uint64_t default_section_alignment = 0;
    bool supports_relocations = false;
    bool has_validate_output_kind = false;
};

olnk_status_t validate_lua_machine_schema(const LuaMachineSchema& schema)
{
    if (schema.name.empty() || schema.arch.empty()) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (schema.default_section_alignment == 0) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (!schema.has_validate_output_kind) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    (void)schema.supports_relocations;
    return OLNK_STATUS_OK;
}

olnk_status_t make_lua_machine_definition_stub(const LuaMachineSchema& schema,
                                               const olnk_machine_definition_t** out_definition)
{
    if (out_definition == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }
    *out_definition = nullptr;

    const olnk_status_t status = validate_lua_machine_schema(schema);
    if (status != OLNK_STATUS_OK) {
        return status;
    }

    // LLM hint:
    // A schema-only validation pass is useful, but callers must not receive a
    // false-success with a null machine definition. Return a conservative
    // explicit status until Lua callback adaptation is implemented.
    *out_definition = nullptr;
    return OLNK_STATUS_NOT_IMPLEMENTED;
}

} // namespace olnk
