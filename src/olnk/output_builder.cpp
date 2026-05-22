// LLM / maintainer hints:
// - Build final internal image/layout from resolved sections/symbols.
// - Keep generic model separate from format-specific serialization concerns.
// - Preserve deterministic ordering in emitted structures.
// - Public ABI does not currently expose output-image internals; this file
//   provides a conservative, deterministic scaffold for future integration.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace olnk {

enum class OutputSectionKind : uint8_t {
    kUnknown = 0,
    kCode = 1,
    kReadOnlyData = 2,
    kData = 3,
    kBss = 4,
    kTls = 5,
    kDebug = 6
};

struct OutputSectionInput {
    std::string name;
    OutputSectionKind kind = OutputSectionKind::kUnknown;
    uint64_t size = 0;
    uint64_t alignment = 1;
    uint32_t input_ordinal = 0;
};

struct OutputLayoutEntry {
    std::string name;
    OutputSectionKind kind = OutputSectionKind::kUnknown;
    uint64_t size = 0;
    uint64_t alignment = 1;
    uint64_t file_offset = 0;
};

struct OutputImage {
    uint64_t base_offset = 0;
    std::vector<OutputLayoutEntry> sections;
};

static uint64_t align_up(uint64_t value, uint64_t alignment)
{
    if (alignment <= 1) {
        return value;
    }

    const uint64_t remainder = value % alignment;
    if (remainder == 0) {
        return value;
    }
    return value + (alignment - remainder);
}

class OutputBuilder {
public:
    void add_section(OutputSectionInput section)
    {
        inputs_.push_back(std::move(section));
    }

    bool build(uint64_t base_offset, OutputImage* out_image)
    {
        if (out_image == nullptr) {
            return false;
        }

        out_image->base_offset = base_offset;
        out_image->sections.clear();

        normalize_inputs();

        uint64_t cursor = base_offset;
        out_image->sections.reserve(inputs_.size());

        for (const OutputSectionInput& input : inputs_) {
            const uint64_t aligned_cursor = align_up(cursor, input.alignment);
            if (aligned_cursor < cursor) {
                return false;
            }

            OutputLayoutEntry entry;
            entry.name = input.name;
            entry.kind = input.kind;
            entry.size = input.size;
            entry.alignment = input.alignment;
            entry.file_offset = aligned_cursor;
            out_image->sections.push_back(std::move(entry));

            cursor = aligned_cursor + input.size;
            if (cursor < aligned_cursor) {
                return false;
            }
        }

        return true;
    }

private:
    void normalize_inputs()
    {
        for (OutputSectionInput& input : inputs_) {
            if (input.alignment == 0) {
                input.alignment = 1;
            }
        }

        std::stable_sort(inputs_.begin(), inputs_.end(),
                         [](const OutputSectionInput& lhs, const OutputSectionInput& rhs) {
                             if (lhs.kind != rhs.kind) {
                                 return static_cast<uint8_t>(lhs.kind) < static_cast<uint8_t>(rhs.kind);
                             }
                             if (lhs.name != rhs.name) {
                                 return lhs.name < rhs.name;
                             }
                             return lhs.input_ordinal < rhs.input_ordinal;
                         });
    }

    std::vector<OutputSectionInput> inputs_;
};

} // namespace olnk
