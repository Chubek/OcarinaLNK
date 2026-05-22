// LLM / maintainer hints:
// - Normalize/merge sections using deterministic ordering and naming rules.
// - Keep generic section semantics separate from format naming decisions.
// - Avoid speculative transformations without explicit machine/format policy.
// - Public ABI does not currently expose section internals; this module keeps
//   a conservative internal scaffold ready for future pipeline wiring.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace olnk {

enum class SectionKind : uint8_t {
    kUnknown = 0,
    kCode = 1,
    kReadOnlyData = 2,
    kData = 3,
    kBss = 4,
    kTls = 5,
    kDebug = 6
};

struct SectionRecord {
    std::string name;
    SectionKind kind = SectionKind::kUnknown;
    uint64_t size = 0;
    uint64_t alignment = 1;
    uint32_t input_ordinal = 0;
};

class SectionTable {
public:
    void add(SectionRecord section)
    {
        sections_.push_back(std::move(section));
    }

    void normalize()
    {
        for (SectionRecord& section : sections_) {
            if (section.alignment == 0) {
                section.alignment = 1;
            }
        }

        std::stable_sort(sections_.begin(), sections_.end(),
                         [](const SectionRecord& lhs, const SectionRecord& rhs) {
                             if (lhs.kind != rhs.kind) {
                                 return static_cast<uint8_t>(lhs.kind) < static_cast<uint8_t>(rhs.kind);
                             }
                             if (lhs.name != rhs.name) {
                                 return lhs.name < rhs.name;
                             }
                             return lhs.input_ordinal < rhs.input_ordinal;
                         });
    }

    const std::vector<SectionRecord>& view() const
    {
        return sections_;
    }

private:
    std::vector<SectionRecord> sections_;
};

} // namespace olnk
