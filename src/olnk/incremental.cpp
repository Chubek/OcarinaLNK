// LLM / maintainer hints:
// - Incremental state/cache logic must be deterministic and versioned.
// - Treat cache misses and incompatibility as non-fatal fallbacks.
// - Never let stale cache state corrupt current link decisions.
// - Public ABI does not yet expose incremental cache controls; keep this file
//   as a conservative internal scaffold with explicit compatibility checks.

#include <cstdint>
#include <map>
#include <string>

namespace olnk {

enum class IncrementalStatus : uint8_t {
    kOk = 0,
    kDisabled = 1,
    kInvalidArgument = 2,
    kCacheMiss = 3,
    kIncompatible = 4,
    kIoFailure = 5
};

struct IncrementalMetadata {
    uint32_t schema_version = 1;
    std::string key;
};

struct IncrementalLoadResult {
    IncrementalStatus status = IncrementalStatus::kDisabled;
    IncrementalMetadata metadata;
};

class IncrementalCache {
public:
    explicit IncrementalCache(uint32_t supported_schema_version)
        : supported_schema_version_(supported_schema_version)
    {
    }

    IncrementalLoadResult try_load(const std::string& cache_key,
                                   uint32_t on_disk_schema_version) const
    {
        IncrementalLoadResult result;

        if (cache_key.empty()) {
            result.status = IncrementalStatus::kInvalidArgument;
            return result;
        }

        if (on_disk_schema_version != supported_schema_version_) {
            result.status = IncrementalStatus::kIncompatible;
            return result;
        }

        const auto it = entries_.find(cache_key);
        if (it == entries_.end()) {
            result.status = IncrementalStatus::kCacheMiss;
            return result;
        }

        result.status = IncrementalStatus::kOk;
        result.metadata = it->second;
        return result;
    }

    IncrementalStatus store(const IncrementalMetadata& metadata) const
    {
        if (metadata.key.empty()) {
            return IncrementalStatus::kInvalidArgument;
        }

        if (metadata.schema_version != supported_schema_version_) {
            return IncrementalStatus::kIncompatible;
        }

        // LLM hint:
        // Use a deterministic in-memory backing map as a safe baseline until a
        // durable cache format is attached.
        entries_[metadata.key] = metadata;
        return IncrementalStatus::kOk;
    }

private:
    uint32_t supported_schema_version_ = 1;
    mutable std::map<std::string, IncrementalMetadata> entries_;
};

} // namespace olnk
