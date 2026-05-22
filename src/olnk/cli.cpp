// LLM / maintainer hints:
// - This CLI module demonstrates Klyspec capabilities (registry/parser/plugins/subcommands/profiles/IPC/DSL)
//   while keeping linker behavior behind the public C ABI.
// - Do not invent private linker internals; only call exported olnk_* APIs.
// - Keep argument handling deterministic: stable ordering, explicit defaults, and clear diagnostics.

#include <olnk/olnk-api.h>

#include "Klyspec-IPC.hpp"
#include "Klyspec-Plugin.hpp"
#include "Klyspec-Profiles.hpp"
#include "Klyspec-Subcommand.hpp"
#include "Klyspec.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace olnk {
namespace {

using klyspec::ArgumentKind;
using klyspec::ArgumentSpec;
using klyspec::CommandSpec;
using klyspec::KlyCLIService;
using klyspec::ParseResult;
using klyspec::Registry;
using klyspec::ValuePolicy;

struct ProcessState {
  olnk_context_t* context{nullptr};
  olnk_config_t* config{nullptr};
  olnk_session_t* session{nullptr};

  ~ProcessState() {
    if (session != nullptr) {
      olnk_session_destroy(session);
    }
    if (config != nullptr) {
      olnk_config_destroy(config);
    }
    if (context != nullptr) {
      olnk_context_destroy(context);
    }
  }
};

class CliBehaviorPlugin final : public klyspec::Plugin {
public:
  std::string name() const override { return "olnk-cli-behavior"; }

  void extend_arguments(Registry& registry) override {
    const klyspec::Flag dry_run("dry_run", {"--dry-run"});
    const klyspec::Switch mode("mode", {"--mode"});
    const klyspec::Option profile("profile", {"--profile"});

    const ArgumentSpec* existing = registry.lookup_argument("link", "dry_run");
    if (existing == nullptr) {
      ArgumentSpec spec = dry_run.spec();
      spec.help = "Parse and validate, but skip linker execution";
      registry.register_argument("link", std::move(spec));
    }

    existing = registry.lookup_argument("link", "mode");
    if (existing == nullptr) {
      ArgumentSpec spec = mode.spec();
      spec.default_value = std::string("strict");
      spec.help = "Optional CLI mode switch (strict|compat)";
      registry.register_argument("link", std::move(spec));
    }

    existing = registry.lookup_argument("link", "profile");
    if (existing == nullptr) {
      ArgumentSpec spec = profile.spec();
      spec.help = "Profile file path for config preload";
      registry.register_argument("link", std::move(spec));
    }
  }

  void before_parse(std::vector<std::string>& argv) override {
    // Expand a single shorthand token deterministically before parse.
    for (std::string& token : argv) {
      if (token == "-vv") {
        token = "--trace";
      }
    }
  }

  void after_parse(ParseResult& result) override {
    if (result.values.contains("verbose") && result.values.contains("trace")) {
      result.diagnostics.push_back("--verbose and --trace both set; using trace level");
    }
  }

  std::optional<int> intercept_dispatch(const ParseResult& result) override {
    if (result.values.contains("dry_run")) {
      std::cout << "dry-run: parse/config validation complete; skipping linker session run\n";
      return 0;
    }
    return std::nullopt;
  }

  void after_dispatch(int code) override {
    if (code != 0) {
      std::cerr << "cli-plugin: dispatch finished with non-zero exit code " << code << "\n";
    }
  }
};

class InspectSubcommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "olnk.native.inspect"; }
  std::string name() const override { return "inspect"; }
  int execute(const std::vector<std::string>& args) override {
    std::cout << "inspect subcommand (native):";
    for (const std::string& arg : args) {
      std::cout << ' ' << arg;
    }
    std::cout << "\n";
    return 0;
  }
};

class ProfilesSubcommand final : public klyspec::NativeSubcommand {
public:
  std::string id() const override { return "olnk.native.profiles"; }
  std::string name() const override { return "profiles"; }
  int execute(const std::vector<std::string>&) override {
    std::cout << "supported profile formats: json, xml, yaml, sexpr\n";
    return 0;
  }
};

const char* status_string(olnk_status_t status) {
  const char* value = olnk_status_to_string(status);
  return value == nullptr ? "unknown-status" : value;
}

bool has_value(const ParseResult& parsed, const std::string& key) {
  return parsed.values.find(key) != parsed.values.end();
}

const std::string* first_value(const ParseResult& parsed, const std::string& key) {
  auto it = parsed.values.find(key);
  if (it == parsed.values.end() || it->second.empty()) {
    return nullptr;
  }
  return &it->second.front();
}

const std::vector<std::string>* values_for(const ParseResult& parsed, const std::string& key) {
  auto it = parsed.values.find(key);
  return (it == parsed.values.end()) ? nullptr : &it->second;
}

void print_help(const CommandSpec& cmd) {
  std::cout << "OcarinaLNK (olnk)\n"
            << "Usage:\n"
            << "  olnk [options] <input...>\n"
            << "  olnk inspect <args...>\n"
            << "  olnk profiles\n\n"
            << cmd.help << "\n\n"
            << "Important options:\n"
            << "  -o, --output <path>\n"
            << "      --format <name>\n"
            << "      --machine <name>\n"
            << "      --output-kind <kind>\n"
            << "      --profile <path>\n"
            << "      --profile-format <json|xml|yaml|sexpr>\n"
            << "      --ipc-signal / --ipc-socket\n"
            << "      --dry-run\n";
}

Registry build_registry() {
  Registry r;
  r.register_command(CommandSpec{
      .name = "link",
      .help = "Link object files and libraries through OcarinaLNK C ABI.",
  });

  r.register_argument("link", ArgumentSpec{"help", ArgumentKind::flag, ValuePolicy::none, {"-h", "--help"}, "Show help", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"version", ArgumentKind::flag, ValuePolicy::none, {"--version"}, "Show version", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"verbose", ArgumentKind::flag, ValuePolicy::none, {"--verbose"}, "Verbose logs", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"trace", ArgumentKind::flag, ValuePolicy::none, {"--trace"}, "Trace logs", std::nullopt, false});

  r.register_argument("link", ArgumentSpec{"output", ArgumentKind::option, ValuePolicy::required, {"-o", "--output"}, "Output path", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"map", ArgumentKind::option, ValuePolicy::required, {"--map"}, "Map output path", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"entry", ArgumentKind::option, ValuePolicy::required, {"--entry"}, "Entry symbol", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"format", ArgumentKind::option, ValuePolicy::required, {"--format"}, "Output format", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"machine", ArgumentKind::option, ValuePolicy::required, {"--machine"}, "Machine target", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"script", ArgumentKind::option, ValuePolicy::required, {"--script"}, "Linker script path", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"output_kind", ArgumentKind::option, ValuePolicy::required, {"--output-kind"}, "Output kind", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"threads", ArgumentKind::option, ValuePolicy::required, {"-j", "--threads"}, "Thread count", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"profile_format", ArgumentKind::option, ValuePolicy::required, {"--profile-format"}, "Profile format", std::optional<std::string>("json"), false});

  r.register_argument("link", ArgumentSpec{"library_path", ArgumentKind::repeatable, ValuePolicy::required, {"-L", "--lib-path"}, "Library search path", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"library", ArgumentKind::repeatable, ValuePolicy::required, {"-l", "--lib"}, "Link library", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"plugin", ArgumentKind::repeatable, ValuePolicy::required, {"--plugin"}, "Plugin path", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"define", ArgumentKind::repeatable, ValuePolicy::required, {"-D", "--define"}, "Define key=value", std::nullopt, false});

  // Klyspec positional/variadic/key_value kinds are registered to exercise advanced validation paths.
  r.register_argument("link", ArgumentSpec{"input0", ArgumentKind::positional, ValuePolicy::required, {}, "Primary input", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"inputs", ArgumentKind::variadic, ValuePolicy::required, {}, "Additional inputs", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"meta", ArgumentKind::key_value, ValuePolicy::optional, {"--meta"}, "Metadata hint", std::nullopt, false});

  r.register_argument("link", ArgumentSpec{"incremental", ArgumentKind::flag, ValuePolicy::none, {"--incremental"}, "Enable incremental mode", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"debug_info", ArgumentKind::flag, ValuePolicy::none, {"--debug-info"}, "Enable debug info", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"ipc_signal", ArgumentKind::flag, ValuePolicy::none, {"--ipc-signal"}, "Enable IPC signal mode", std::nullopt, false});
  r.register_argument("link", ArgumentSpec{"ipc_socket", ArgumentKind::flag, ValuePolicy::none, {"--ipc-socket"}, "Enable IPC local socket mode", std::nullopt, false});

  r.register_alias("link", "olnk");
  return r;
}

std::optional<klyspec::KlyProfileLoader::Format> parse_profile_format(std::string_view value) {
  if (value == "json") return klyspec::KlyProfileLoader::Format::json;
  if (value == "xml") return klyspec::KlyProfileLoader::Format::xml;
  if (value == "yaml") return klyspec::KlyProfileLoader::Format::yaml;
  if (value == "sexpr") return klyspec::KlyProfileLoader::Format::sexpr;
  return std::nullopt;
}

bool apply_string_setter(ParseResult const& parsed,
                         olnk_config_t* config,
                         const std::string& key,
                         olnk_status_t (*setter)(olnk_config_t*, const char*),
                         std::string_view label,
                         std::ostream& err) {
  const std::string* value = first_value(parsed, key);
  if (value == nullptr) {
    return true;
  }
  const olnk_status_t s = setter(config, value->c_str());
  if (s != OLNK_STATUS_OK) {
    err << "error: failed to set " << label << ": " << status_string(s) << "\n";
    return false;
  }
  return true;
}

bool apply_profile(ParseResult const& parsed, olnk_config_t* config, std::ostream& err) {
  const std::string* profile_path = first_value(parsed, "profile");
  if (profile_path == nullptr) {
    return true;
  }

  std::ifstream in(*profile_path);
  if (!in) {
    err << "error: cannot open profile file: " << *profile_path << "\n";
    return false;
  }
  std::stringstream buffer;
  buffer << in.rdbuf();

  const std::string format_string = first_value(parsed, "profile_format") == nullptr
                                        ? "json"
                                        : *first_value(parsed, "profile_format");
  auto format = parse_profile_format(format_string);
  if (!format.has_value()) {
    err << "error: invalid profile format: " << format_string << "\n";
    return false;
  }

  auto loaded = klyspec::KlyProfileLoader::load(*format, buffer.str());
  if (!loaded.has_value()) {
    err << "error: failed to parse profile file\n";
    return false;
  }

  for (const auto& [key, value] : *loaded) {
    if (key == "output") {
      if (olnk_config_set_output_path(config, value.c_str()) != OLNK_STATUS_OK) return false;
    } else if (key == "format") {
      if (olnk_config_set_format(config, value.c_str()) != OLNK_STATUS_OK) return false;
    } else if (key == "machine") {
      if (olnk_config_set_machine(config, value.c_str()) != OLNK_STATUS_OK) return false;
    } else if (key == "script") {
      if (olnk_config_set_script_path(config, value.c_str()) != OLNK_STATUS_OK) return false;
    } else if (key.rfind("define.", 0) == 0) {
      const std::string define_key = key.substr(7);
      if (!define_key.empty()) {
        olnk_config_define(config, define_key.c_str(), value.c_str());
      }
    }
  }
  return true;
}

bool apply_config(olnk_config_t* config, const ParseResult& parsed, std::ostream& err) {
  if (!apply_profile(parsed, config, err)) return false;

  if (!apply_string_setter(parsed, config, "output", &olnk_config_set_output_path, "output", err)) return false;
  if (!apply_string_setter(parsed, config, "map", &olnk_config_set_map_path, "map", err)) return false;
  if (!apply_string_setter(parsed, config, "entry", &olnk_config_set_entry_symbol, "entry", err)) return false;
  if (!apply_string_setter(parsed, config, "format", &olnk_config_set_format, "format", err)) return false;
  if (!apply_string_setter(parsed, config, "machine", &olnk_config_set_machine, "machine", err)) return false;
  if (!apply_string_setter(parsed, config, "script", &olnk_config_set_script_path, "script", err)) return false;

  if (const std::string* kind = first_value(parsed, "output_kind")) {
    olnk_output_kind_t mapped = OLNK_OUTPUT_EXECUTABLE;
    if (*kind == "executable") mapped = OLNK_OUTPUT_EXECUTABLE;
    else if (*kind == "shared") mapped = OLNK_OUTPUT_SHARED_LIBRARY;
    else if (*kind == "static") mapped = OLNK_OUTPUT_STATIC_IMAGE;
    else if (*kind == "relocatable") mapped = OLNK_OUTPUT_RELOCATABLE;
    else if (*kind == "binary") mapped = OLNK_OUTPUT_BINARY;
    else {
      err << "error: invalid --output-kind: " << *kind << "\n";
      return false;
    }
    if (olnk_config_set_output_kind(config, mapped) != OLNK_STATUS_OK) {
      err << "error: failed to set output kind\n";
      return false;
    }
  }

  if (const std::string* threads = first_value(parsed, "threads")) {
    char* end = nullptr;
    const long count = std::strtol(threads->c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || count < 0) {
      err << "error: --threads expects non-negative integer\n";
      return false;
    }
    if (olnk_config_set_thread_count(config, static_cast<uint32_t>(count)) != OLNK_STATUS_OK) {
      err << "error: failed to set threads\n";
      return false;
    }
  }

  if (olnk_config_set_incremental(config, has_value(parsed, "incremental") ? 1 : 0) != OLNK_STATUS_OK) {
    err << "error: failed to set incremental mode\n";
    return false;
  }
  if (olnk_config_set_debug_info(config, has_value(parsed, "debug_info") ? 1 : 0) != OLNK_STATUS_OK) {
    err << "error: failed to set debug-info mode\n";
    return false;
  }

  if (const std::vector<std::string>* defs = values_for(parsed, "define")) {
    for (const std::string& kv : *defs) {
      const std::size_t eq = kv.find('=');
      if (eq == std::string::npos || eq == 0 || eq + 1 >= kv.size()) {
        err << "error: invalid define: " << kv << "\n";
        return false;
      }
      const std::string key = kv.substr(0, eq);
      const std::string value = kv.substr(eq + 1);
      if (olnk_config_define(config, key.c_str(), value.c_str()) != OLNK_STATUS_OK) {
        err << "error: failed define: " << key << "\n";
        return false;
      }
    }
  }

  if (const std::vector<std::string>* libs = values_for(parsed, "library")) {
    for (const std::string& lib : *libs) {
      if (olnk_config_add_library(config, lib.c_str()) != OLNK_STATUS_OK) return false;
    }
  }
  if (const std::vector<std::string>* lib_paths = values_for(parsed, "library_path")) {
    for (const std::string& p : *lib_paths) {
      if (olnk_config_add_library_path(config, p.c_str()) != OLNK_STATUS_OK) return false;
    }
  }
  if (const std::vector<std::string>* plugins = values_for(parsed, "plugin")) {
    for (const std::string& p : *plugins) {
      if (olnk_config_add_plugin(config, p.c_str()) != OLNK_STATUS_OK) return false;
    }
  }

  for (const std::string& input : parsed.positionals) {
    if (olnk_config_add_input_file(config, input.c_str()) != OLNK_STATUS_OK) return false;
  }
  return true;
}

int print_diagnostics(olnk_session_t* session) {
  const size_t n = olnk_session_diagnostic_count(session);
  for (size_t i = 0; i < n; ++i) {
    const olnk_diagnostic_t* d = olnk_session_diagnostic_at(session, i);
    if (d == nullptr) {
      continue;
    }
    std::cerr << "diagnostic[" << i << "]: "
              << (olnk_diagnostic_message(d) != nullptr ? olnk_diagnostic_message(d) : "<no-message>")
              << "\n";
  }
  return n == 0 ? 0 : 1;
}

void configure_ipc(klyspec::KlyIPCService& ipc, const ParseResult& parsed) {
  if (has_value(parsed, "ipc_socket")) {
    ipc.enable<klyspec::IPC::LocalSocket>([](const ParseResult& r) { return r.ok ? 0 : 1; });
  } else {
    ipc.enable<klyspec::IPC::Signal>([](const ParseResult& r) { return r.ok ? 0 : 2; });
  }
}

void emit_ipc_snapshot(const ParseResult& parsed) {
  if (!has_value(parsed, "ipc_signal") && !has_value(parsed, "ipc_socket")) {
    return;
  }
  klyspec::KlyIPCService ipc;
  configure_ipc(ipc, parsed);
  auto payload = ipc.serialize(parsed);
  if (payload.is_ok()) {
    const auto bytes = payload.unwrap();
    std::string text(bytes.begin(), bytes.end());
    std::cout << "ipc-payload: " << text << "\n";
  }
  auto dispatched = ipc.dispatch(parsed);
  if (dispatched.has_value()) {
    std::cout << "ipc-dispatch-code: " << *dispatched << "\n";
  }
}

void run_klytmk_probe() {
  static constexpr const char* dsl =
      "command \"link\" {\n"
      "  help-string { OcarinaLNK link pipeline }\n"
      "};\n";
  const auto parsed = klyspec::parse_klytmk(dsl);
  if (!parsed.ok || !parsed.ast.has_value() || parsed.ast->nodes.empty()) {
    std::cerr << "warning: embedded klytmk DSL probe failed\n";
  }
}

}  // namespace

int run_cli(int argc, char** argv) {
  run_klytmk_probe();

  klyspec::SubcommandRegistry subcommands;
  subcommands.register_subcommand(std::make_shared<InspectSubcommand>());
  subcommands.register_discovery_hook([] {
    return std::vector<std::shared_ptr<klyspec::NativeSubcommand>>{
        std::make_shared<ProfilesSubcommand>()};
  });
  subcommands.discover();

  std::vector<std::string> args;
  args.reserve(argc > 1 ? static_cast<size_t>(argc - 1) : 0u);
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  // Fast-path global help/version so positional validation does not block UX.
  for (const std::string& token : args) {
    if (token == "--version") {
      std::cout << olnk_version_string() << "\n";
      return 0;
    }
    if (token == "--help" || token == "-h") {
      Registry quick_registry = build_registry();
      const CommandSpec* cmd = quick_registry.lookup("link");
      if (cmd != nullptr) {
        print_help(*cmd);
      }
      return 0;
    }
  }

  if (!args.empty()) {
    const std::string& first = args.front();
    if (first == "inspect" || first == "profiles" || first == "olnk.native.inspect" || first == "olnk.native.profiles") {
      std::vector<std::string> subargs(args.begin() + 1, args.end());
      return subcommands.dispatch(first, subargs);
    }
  }

  Registry registry = build_registry();
  klyspec::PluginRegistry plugin_registry;
  plugin_registry.register_plugin(std::make_shared<CliBehaviorPlugin>());
  plugin_registry.extend_arguments(registry);

  KlyCLIService cli(registry);
  plugin_registry.before_parse(args);
  ParseResult parsed = cli.parse("link", args);
  plugin_registry.after_parse(parsed);

  if (!parsed.ok) {
    for (const std::string& diag : parsed.diagnostics) {
      std::cerr << "error: " << diag << "\n";
    }
    return 2;
  }

  if (has_value(parsed, "help")) {
    const CommandSpec* cmd = registry.lookup("link");
    if (cmd != nullptr) {
      print_help(*cmd);
    }
    return 0;
  }

  if (has_value(parsed, "version")) {
    std::cout << olnk_version_string() << "\n";
    return 0;
  }

  emit_ipc_snapshot(parsed);

  ProcessState state;
  state.context = olnk_context_create();
  if (state.context == nullptr) {
    std::cerr << "fatal: failed to create context\n";
    return 1;
  }

  if (has_value(parsed, "trace")) {
    olnk_context_set_log_level(state.context, OLNK_LOG_TRACE);
  } else if (has_value(parsed, "verbose")) {
    olnk_context_set_log_level(state.context, OLNK_LOG_DEBUG);
  }

  state.config = olnk_config_create();
  if (state.config == nullptr) {
    std::cerr << "fatal: failed to create config\n";
    return 1;
  }

  if (!apply_config(state.config, parsed, std::cerr)) {
    return 2;
  }

  plugin_registry.before_dispatch(parsed);
  if (auto intercepted = plugin_registry.intercept_dispatch(parsed); intercepted.has_value()) {
    plugin_registry.after_dispatch(*intercepted);
    return *intercepted;
  }

  state.session = olnk_session_create(state.context, state.config);
  if (state.session == nullptr) {
    std::cerr << "fatal: failed to create session\n";
    return 1;
  }

  olnk_result_t* result = nullptr;
  const olnk_status_t status = olnk_session_run(state.session, &result);
  int exit_code = 0;
  if (status != OLNK_STATUS_OK) {
    std::cerr << "error: link failed: " << status_string(status) << "\n";
    print_diagnostics(state.session);
    exit_code = 1;
  } else {
    print_diagnostics(state.session);
  }

  olnk_result_destroy(result);
  plugin_registry.after_dispatch(exit_code);
  return exit_code;
}

}  // namespace olnk
