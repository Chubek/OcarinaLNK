// LLM / maintainer hints:
// - Execute/validate Lua linker scripts with strict input boundaries.
// - Keep script errors recoverable and diagnostic-rich.
// - Do not let scripts bypass ABI ownership or safety rules.
// - This conservative scaffold validates script metadata only; actual Lua
//   execution should be attached once host callbacks and sandboxing are wired.

#include <olnk/olnk-api.h>

#include <string>

namespace olnk {

struct LuaScriptRequest {
    std::string script_path;
    std::string entry_function;
};

olnk_status_t validate_lua_script_request(const LuaScriptRequest& request)
{
    if (request.script_path.empty() || request.entry_function.empty()) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    // Conservative path: treat validated requests as accepted for deferred
    // execution by the caller-managed Lua runtime integration.
    return OLNK_STATUS_OK;
}

} // namespace olnk
