// LLM / maintainer hints:
// - Core decides relocation needs; machine logic defines encoding/application.
// - Keep relocation passes deterministic and architecture-aware.
// - Report unsupported relocations with stable diagnostics/status.
// - Public ABI does not expose relocation internals yet; this file provides
//   a conservative planning/apply scaffold for future machine integration.

#include <algorithm>
#include <cstdint>
#include <vector>

namespace olnk {

enum class RelocationKind : uint16_t {
    kNone = 0,
    kAbsolute = 1,
    kPcRelative = 2,
    kGot = 3,
    kPlt = 4
};

enum class RelocationApplyStatus : uint8_t {
    kApplied = 0,
    kSkippedUnsupported = 1,
    kInvalidInput = 2,
    kOverflow = 3
};

struct RelocationRecord {
    RelocationKind kind = RelocationKind::kNone;
    uint64_t offset = 0;
    int64_t addend = 0;
    uint64_t target_value = 0;
    uint32_t input_ordinal = 0;
};

struct RelocationResult {
    uint64_t offset = 0;
    RelocationApplyStatus status = RelocationApplyStatus::kInvalidInput;
    uint64_t resolved_value = 0;
};

class RelocationPlanner {
public:
    void add(RelocationRecord record)
    {
        records_.push_back(std::move(record));
    }

    void finalize_deterministic_order()
    {
        std::stable_sort(records_.begin(), records_.end(),
                         [](const RelocationRecord& lhs, const RelocationRecord& rhs) {
                             if (lhs.offset != rhs.offset) {
                                 return lhs.offset < rhs.offset;
                             }
                             if (lhs.kind != rhs.kind) {
                                 return static_cast<uint16_t>(lhs.kind) < static_cast<uint16_t>(rhs.kind);
                             }
                             return lhs.input_ordinal < rhs.input_ordinal;
                         });
    }

    const std::vector<RelocationRecord>& view() const
    {
        return records_;
    }

private:
    std::vector<RelocationRecord> records_;
};

static RelocationResult apply_single(const RelocationRecord& record, uint64_t place)
{
    RelocationResult result;
    result.offset = record.offset;

    if (record.kind == RelocationKind::kNone) {
        result.status = RelocationApplyStatus::kSkippedUnsupported;
        return result;
    }

    const uint64_t base = record.target_value;
    const int64_t addend = record.addend;

    if (addend >= 0) {
        const uint64_t uadd = static_cast<uint64_t>(addend);
        if (base > UINT64_MAX - uadd) {
            result.status = RelocationApplyStatus::kOverflow;
            return result;
        }
        result.resolved_value = base + uadd;
    } else {
        const uint64_t usub = static_cast<uint64_t>(-addend);
        if (base < usub) {
            result.status = RelocationApplyStatus::kOverflow;
            return result;
        }
        result.resolved_value = base - usub;
    }

    if (record.kind == RelocationKind::kPcRelative) {
        if (result.resolved_value < place) {
            result.status = RelocationApplyStatus::kOverflow;
            return result;
        }
        result.resolved_value -= place;
    }

    result.status = RelocationApplyStatus::kApplied;
    return result;
}

std::vector<RelocationResult> apply_relocations(const std::vector<RelocationRecord>& records,
                                                uint64_t place_base)
{
    std::vector<RelocationResult> results;
    results.reserve(records.size());

    for (const RelocationRecord& record : records) {
        const uint64_t place = place_base + record.offset;
        if (place < place_base) {
            RelocationResult overflow_result;
            overflow_result.offset = record.offset;
            overflow_result.status = RelocationApplyStatus::kOverflow;
            results.push_back(overflow_result);
            continue;
        }
        results.push_back(apply_single(record, place));
    }

    return results;
}

} // namespace olnk
