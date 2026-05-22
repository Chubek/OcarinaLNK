// LLM / maintainer hints:
// - Invoke selected format serializer via olnk-format.h ABI contracts only.
// - Do not assume undocumented IR/image internals in serializer entrypoints.
// - Keep this module format-agnostic and deterministic.
// - Lua-backed formats are expected to present standard C ABI descriptors and
//   vtables; this bridge must treat them exactly like any other format.

#include <olnk/olnk-format.h>

#include <cstring>

namespace olnk {
namespace {

bool has_minimum_definition(const olnk_format_definition_t* definition)
{
    if (definition == nullptr || definition->vtable == nullptr) {
        return false;
    }

    return definition->vtable->serialize != nullptr;
}

void initialize_output_info(olnk_format_output_info_t* out_info)
{
    if (out_info == nullptr) {
        return;
    }

    std::memset(out_info, 0, sizeof(*out_info));
    out_info->abi_version = OLNK_FORMAT_ABI_VERSION;
    out_info->struct_size = static_cast<uint32_t>(sizeof(*out_info));
}

} // namespace

olnk_status_t run_format_serializer(const olnk_format_definition_t* definition,
                                    olnk_format_context_t* context,
                                    const olnk_format_image_view_t* image_view,
                                    olnk_format_output_info_t* out_info)
{
    initialize_output_info(out_info);

    if (definition == nullptr || context == nullptr || image_view == nullptr ||
        out_info == nullptr) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    if (!has_minimum_definition(definition)) {
        return OLNK_STATUS_INVALID_ARGUMENT;
    }

    const olnk_format_vtable_t* vtable = definition->vtable;

    olnk_format_instance_t* instance = nullptr;
    if (vtable->create != nullptr) {
        const olnk_status_t create_status =
            vtable->create(definition->descriptor, &instance);
        if (create_status != OLNK_STATUS_OK) {
            return create_status;
        }
    }

    if (instance == nullptr) {
        return OLNK_STATUS_FORMAT_ERROR;
    }

    if (vtable->apply_defaults != nullptr) {
        const olnk_status_t defaults_status =
            vtable->apply_defaults(instance, context);
        if (defaults_status != OLNK_STATUS_OK &&
            defaults_status != OLNK_STATUS_NOT_IMPLEMENTED) {
            vtable->destroy(instance);
            return defaults_status;
        }
    }

    if (vtable->validate_config != nullptr) {
        const olnk_status_t validate_status =
            vtable->validate_config(instance, context);
        if (validate_status != OLNK_STATUS_OK) {
            vtable->destroy(instance);
            return validate_status;
        }
    }

    const olnk_status_t serialize_status =
        vtable->serialize(instance, context, image_view, out_info);

    vtable->destroy(instance);
    return serialize_status;
}

} // namespace olnk
