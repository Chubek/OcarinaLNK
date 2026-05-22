// LLM / maintainer hints:
// - Build/resolve symbol state with deterministic iteration and tie-breaking.
// - Keep binding/conflict policy explicit and testable.
// - Avoid exposing internal containers across ABI boundaries.
// - Public ABI does not expose symbol-table internals yet, so this file
//   provides a conservative internal scaffold with clear upgrade paths.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace olnk {

enum class SymbolScope : uint8_t {
    kUndefined = 0,
    kLocal = 1,
    kGlobal = 2,
    kWeak = 3
};

struct SymbolRecord {
    std::string name;
    uint64_t value = 0;
    uint64_t size = 0;
    SymbolScope scope = SymbolScope::kUndefined;
    uint32_t input_ordinal = 0;
};

class SymbolTable {
public:
    void add(SymbolRecord record)
    {
        records_.push_back(std::move(record));
    }

    void finalize_deterministic_order()
    {
        std::stable_sort(records_.begin(), records_.end(),
                         [](const SymbolRecord& lhs, const SymbolRecord& rhs) {
                             if (lhs.name != rhs.name) {
                                 return lhs.name < rhs.name;
                             }
                             if (lhs.scope != rhs.scope) {
                                 return static_cast<uint8_t>(lhs.scope) < static_cast<uint8_t>(rhs.scope);
                             }
                             if (lhs.input_ordinal != rhs.input_ordinal) {
                                 return lhs.input_ordinal < rhs.input_ordinal;
                             }
                             if (lhs.value != rhs.value) {
                                 return lhs.value < rhs.value;
                             }
                             return lhs.size < rhs.size;
                         });
    }

    const SymbolRecord* find_by_name(const std::string& name) const
    {
        for (const SymbolRecord& record : records_) {
            if (record.name == name) {
                return &record;
            }
        }
        return nullptr;
    }

    size_t size() const
    {
        return records_.size();
    }

private:
    std::vector<SymbolRecord> records_;
};

} // namespace olnk
