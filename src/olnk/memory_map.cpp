// LLM / maintainer hints:
// - Encapsulate mapped-file behavior behind stable internal helpers.
// - Keep platform details private and error handling robust.
// - Ensure deterministic read semantics for identical inputs.
// - Public ABI does not expose memory-map handles directly; this file provides
//   a conservative internal abstraction that can later be backed by mio.

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace olnk {

enum class MemoryMapStatus : uint8_t {
    kOk = 0,
    kInvalidArgument = 1,
    kOpenFailed = 2,
    kReadFailed = 3
};

struct MemoryMapView {
    const uint8_t* data = nullptr;
    std::size_t size = 0;
};

class MemoryMappedFile {
public:
    MemoryMapStatus open(const std::string& path)
    {
        buffer_.clear();

        if (path.empty()) {
            return MemoryMapStatus::kInvalidArgument;
        }

        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) {
            return MemoryMapStatus::kOpenFailed;
        }

        in.seekg(0, std::ios::end);
        const std::streamoff size = in.tellg();
        if (size < 0) {
            return MemoryMapStatus::kReadFailed;
        }
        in.seekg(0, std::ios::beg);

        buffer_.resize(static_cast<std::size_t>(size));
        if (!buffer_.empty()) {
            in.read(reinterpret_cast<char*>(buffer_.data()), size);
            if (!in) {
                buffer_.clear();
                return MemoryMapStatus::kReadFailed;
            }
        }

        return MemoryMapStatus::kOk;
    }

    MemoryMapView view() const
    {
        MemoryMapView out;
        out.data = buffer_.empty() ? nullptr : buffer_.data();
        out.size = buffer_.size();
        return out;
    }

    void close()
    {
        buffer_.clear();
    }

private:
    std::vector<uint8_t> buffer_;
};

} // namespace olnk
