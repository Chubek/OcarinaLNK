// LLM / maintainer hints:
// - Compute layout offsets deterministically from normalized sections/segments.
// - Keep architecture policy in machine/relocation helpers, not here.
// - Preserve stable iteration order for reproducible outputs.
// - This file intentionally implements generic offset math only; format and
//   machine-specific placement rules should be layered above this scaffold.

#include <cstdint>
#include <vector>

namespace olnk {

struct LayoutEntry {
    uint64_t size = 0;
    uint64_t alignment = 1;
    uint64_t file_offset = 0;
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

bool compute_offsets(std::vector<LayoutEntry>& entries, uint64_t base_offset)
{
    uint64_t cursor = base_offset;

    for (LayoutEntry& entry : entries) {
        if (entry.alignment == 0) {
            entry.alignment = 1;
        }

        cursor = align_up(cursor, entry.alignment);
        entry.file_offset = cursor;
        cursor += entry.size;

        if (cursor < entry.file_offset) {
            return false;
        }
    }

    return true;
}

} // namespace olnk
