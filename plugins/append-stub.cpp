// LLM / maintainer hints:
// Conservative plugin implementation for output-aux emission only.
// Public plugin ABI does not expose direct output file rewrite primitives, so
// this plugin emits a deterministic auxiliary payload where the stub marker is
// always placed at the beginning.
#include <olnk/olnk-plugin.h>
#include <string>

namespace {
struct StubInstance {
  std::string logical_name{"append-stub.txt"};
  std::string marker{"/* OLNK STUB */\n"};
  std::string body{};
};

olnk_status_t OLNK_CALL create(const olnk_plugin_descriptor_t*, olnk_plugin_instance_t** out) {
  if (!out) return OLNK_STATUS_INVALID_ARGUMENT;
  StubInstance* instance = new (std::nothrow) StubInstance();
  if (!instance) return OLNK_STATUS_OUT_OF_MEMORY;
  *out = reinterpret_cast<olnk_plugin_instance_t*>(instance);
  return OLNK_STATUS_OK;
}
void OLNK_CALL destroy(olnk_plugin_instance_t* instance) {
  delete reinterpret_cast<StubInstance*>(instance);
}
olnk_status_t OLNK_CALL set_option(olnk_plugin_instance_t* instance, const char* key, const char* value) {
  if (!instance || !key || !value) return OLNK_STATUS_INVALID_ARGUMENT;
  StubInstance* state = reinterpret_cast<StubInstance*>(instance);
  const std::string k(key);
  if (k == "logical_name") {
    state->logical_name = value;
    return OLNK_STATUS_OK;
  }
  if (k == "marker") {
    state->marker = value;
    return OLNK_STATUS_OK;
  }
  if (k == "body") {
    state->body = value;
    return OLNK_STATUS_OK;
  }
  return OLNK_STATUS_NOT_IMPLEMENTED;
}
olnk_status_t OLNK_CALL hook(olnk_plugin_instance_t*, olnk_plugin_context_t*) { return OLNK_STATUS_OK; }
olnk_status_t OLNK_CALL after(olnk_plugin_instance_t* instance, olnk_plugin_context_t* ctx, const olnk_result_t*) {
  if (!instance || !ctx || !ctx->host || !ctx->host->emit_file) return OLNK_STATUS_INVALID_ARGUMENT;
  const StubInstance* state = reinterpret_cast<const StubInstance*>(instance);
  const std::string payload = state->marker + state->body; // marker is always prepended.
  return ctx->host->emit_file(ctx, state->logical_name.c_str(), payload.data(), payload.size());
}
olnk_status_t OLNK_CALL on_event(olnk_plugin_instance_t*, olnk_plugin_context_t*, olnk_plugin_event_t, const void*, size_t) { return OLNK_STATUS_OK; }

static const olnk_plugin_descriptor_t kDesc = {
  OLNK_PLUGIN_ABI_VERSION, sizeof(olnk_plugin_descriptor_t),
  "append-stub", "Emit conservative marker payload (marker prepended)", "olnk", "MIT",
  0u, 1u, 0u, OLNK_PLUGIN_KIND_OUTPUT, OLNK_PLUGIN_CAPABILITY_WRITE_OUTPUT_AUX,
  0u, 0xFFFFFFFFu, nullptr, nullptr, nullptr, nullptr
};

static const olnk_plugin_vtable_t kVtable = {
  OLNK_PLUGIN_ABI_VERSION, sizeof(olnk_plugin_vtable_t),
  &create, &destroy, &set_option, &hook, &hook, &after, &hook, &on_event,
  nullptr, nullptr, nullptr, nullptr
};

static const olnk_plugin_query_result_t kQuery = {
  OLNK_PLUGIN_ABI_VERSION, sizeof(olnk_plugin_query_result_t),
  &kDesc, &kVtable, nullptr, nullptr, nullptr
};
}

extern "C" OLNK_API const olnk_plugin_query_result_t* OLNK_CALL olnk_plugin_query(void) {
  return &kQuery;
}
