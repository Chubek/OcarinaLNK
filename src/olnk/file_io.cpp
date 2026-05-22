// LLM / maintainer hints:
// - File read/write helpers should report partial-failure safely.
// - Keep ownership and cleanup explicit for all temporary buffers/files.
// - Avoid platform-specific behavior leaking into ABI-facing layers.
// - Public ABI does not yet expose full file APIs; keep this as an internal,
//   deterministic scaffold that can be wired into higher layers later.

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace olnk {

enum class FileIoStatus : uint8_t {
    kOk = 0,
    kInvalidArgument = 1,
    kOpenFailed = 2,
    kReadFailed = 3,
    kWriteFailed = 4,
    kPartialWrite = 5
};

struct FileReadResult {
    FileIoStatus status = FileIoStatus::kInvalidArgument;
    std::vector<uint8_t> bytes;
};

struct FileWriteResult {
    FileIoStatus status = FileIoStatus::kInvalidArgument;
    std::size_t bytes_written = 0;
};

FileReadResult read_file_all(const std::string& path)
{
    FileReadResult result;
    if (path.empty()) {
        return result;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        result.status = FileIoStatus::kOpenFailed;
        return result;
    }

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) {
        result.status = FileIoStatus::kReadFailed;
        return result;
    }

    in.seekg(0, std::ios::beg);
    result.bytes.resize(static_cast<std::size_t>(size));

    if (!result.bytes.empty()) {
        in.read(reinterpret_cast<char*>(result.bytes.data()), size);
        if (!in) {
            result.bytes.clear();
            result.status = FileIoStatus::kReadFailed;
            return result;
        }
    }

    result.status = FileIoStatus::kOk;
    return result;
}

FileWriteResult write_file_all(const std::string& path,
                               const uint8_t* data,
                               std::size_t size)
{
    FileWriteResult result;
    if (path.empty() || (data == nullptr && size != 0)) {
        return result;
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        result.status = FileIoStatus::kOpenFailed;
        return result;
    }

    if (size != 0) {
        out.write(reinterpret_cast<const char*>(data),
                  static_cast<std::streamsize>(size));
    }

    if (!out.good()) {
        result.status = FileIoStatus::kWriteFailed;
        return result;
    }

    result.status = FileIoStatus::kOk;
    result.bytes_written = size;
    return result;
}

} // namespace olnk
