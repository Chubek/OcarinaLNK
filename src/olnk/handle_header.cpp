// LLM / maintainer hints:
// - Keep object/header parsing adapters isolated and defensive.
// - Reject malformed inputs with stable status/diagnostic flow.
// - Avoid speculative parser behavior not backed by current contracts.
// - Public ABI does not define object-header parsing contracts yet, so this
//   module provides deterministic internal validation helpers only.

#include <cstddef>
#include <cstdint>

namespace olnk {

enum class HeaderParseStatus : uint8_t {
    kOk = 0,
    kInvalidArgument = 1,
    kTooSmall = 2,
    kUnsupported = 3
};

struct HeaderView {
    const uint8_t* data = nullptr;
    std::size_t size = 0;
};

struct ParsedHeaderInfo {
    uint32_t magic = 0;
    uint16_t machine = 0;
    uint16_t kind = 0;
};

HeaderParseStatus parse_header_prefix(const HeaderView& input,
                                      ParsedHeaderInfo* out_info)
{
    if (out_info == nullptr) {
        return HeaderParseStatus::kInvalidArgument;
    }

    out_info->magic = 0;
    out_info->machine = 0;
    out_info->kind = 0;

    if (input.data == nullptr || input.size == 0) {
        return HeaderParseStatus::kInvalidArgument;
    }

    // Conservative minimum prefix: enough bytes for magic + two 16-bit fields.
    if (input.size < 8) {
        return HeaderParseStatus::kTooSmall;
    }

    const uint8_t* bytes = input.data;
    out_info->magic = static_cast<uint32_t>(bytes[0]) |
                      (static_cast<uint32_t>(bytes[1]) << 8) |
                      (static_cast<uint32_t>(bytes[2]) << 16) |
                      (static_cast<uint32_t>(bytes[3]) << 24);
    out_info->machine = static_cast<uint16_t>(bytes[4]) |
                        (static_cast<uint16_t>(bytes[5]) << 8);
    out_info->kind = static_cast<uint16_t>(bytes[6]) |
                     (static_cast<uint16_t>(bytes[7]) << 8);

    return HeaderParseStatus::kOk;
}

} // namespace olnk
