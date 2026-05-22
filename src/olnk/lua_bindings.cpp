#include <sol/sol.hpp>

#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace olnk {

static std::string read_file_text(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("olnk.io.read_file: cannot open file: " + path);
    }

    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string required_string(sol::table spec, const char* key, const char* scope)
{
    const sol::optional<std::string> value = spec[key];
    if (!value || value->empty()) {
        throw std::runtime_error(std::string(scope) + ": required string field missing/empty: " + key);
    }
    return *value;
}

static sol::table normalize_string_list(sol::state_view lua, sol::optional<sol::table> raw)
{
    sol::table out = lua.create_table();
    if (!raw) {
        return out;
    }
    int index = 1;
    for (const auto& kv : *raw) {
        const sol::optional<std::string> item = kv.second.as<sol::optional<std::string>>();
        if (item && !item->empty()) {
            out[index++] = *item;
        }
    }
    return out;
}

static sol::table define_format(sol::this_state ts, sol::table spec)
{
    sol::state_view lua(ts);
    const std::string name = required_string(spec, "name", "olnk.format.define");
    const std::string kind = required_string(spec, "kind", "olnk.format.define");

    sol::table normalized = lua.create_table();
    normalized["name"] = name;
    normalized["kind"] = kind;
    normalized["description"] = spec.get_or("description", std::string{});
    normalized["vendor"] = spec.get_or("vendor", std::string{"olnk"});
    normalized["license"] = spec.get_or("license", std::string{});
    normalized["version"] = spec.get_or("version", std::string{"1.0.0"});
    normalized["capabilities"] = normalize_string_list(lua, spec["capabilities"]);
    normalized["aliases"] = normalize_string_list(lua, spec["aliases"]);
    normalized["defaults"] = spec.get_or("defaults", lua.create_table());

    sol::table olnk = lua["olnk"];
    sol::table registry = olnk["registry"].get_or_create<sol::table>();
    sol::table formats = registry["formats"].get_or_create<sol::table>();
    formats[name] = normalized;
    return normalized;
}

static sol::table define_machine(sol::this_state ts, sol::table spec)
{
    sol::state_view lua(ts);
    const std::string name = required_string(spec, "name", "olnk.machine.define");
    const std::string arch = required_string(spec, "arch", "olnk.machine.define");

    sol::table normalized = lua.create_table();
    normalized["name"] = name;
    normalized["arch"] = arch;
    normalized["description"] = spec.get_or("description", std::string{});
    normalized["endianness"] = spec.get_or("endianness", std::string{"little"});
    normalized["image_class"] = spec.get_or("image_class", 64);
    normalized["aliases"] = normalize_string_list(lua, spec["aliases"]);
    normalized["default_page_alignment"] = spec.get_or("default_page_alignment", 0x1000);
    normalized["default_section_alignment"] = spec.get_or("default_section_alignment", 0x10);
    normalized["supports_relocations"] = spec.get_or("supports_relocations", false);
    normalized["relocation_map"] = spec.get_or("relocation_map", lua.create_table());

    sol::table olnk = lua["olnk"];
    sol::table registry = olnk["registry"].get_or_create<sol::table>();
    sol::table machines = registry["machines"].get_or_create<sol::table>();
    machines[name] = normalized;
    return normalized;
}

void register_olnk_lua(sol::state& lua)
{
    sol::table olnk = lua["olnk"].get_or_create<sol::table>();
    olnk["registry"] = olnk.get_or("registry", lua.create_table());

    sol::table format = lua.create_table();
    format.set_function("define", &define_format);

    sol::table machine = lua.create_table();
    machine.set_function("define", &define_machine);

    sol::table io = lua.create_table();
    io.set_function("read_file", [](const std::string& path) { return read_file_text(path); });

    olnk["format"] = format;
    olnk["machine"] = machine;
    olnk["io"] = io;
}

} // namespace olnk
