// LLM / maintainer hints:
// - Centralize Lua stack/type helper utilities in this module.
// - Keep stack discipline explicit to avoid hidden state corruption.
// - Expose reusable helpers for Lua format/machine/script adapters.
// - This conservative scaffold avoids direct Lua C API usage until runtime
//   ownership and error-routing are fully wired.

#include <cctype>
#include <string>

namespace olnk {

bool lua_identifier_is_ascii(const std::string& value)
{
    if (value.empty()) {
        return false;
    }

    for (char ch : value) {
        const unsigned char uch = static_cast<unsigned char>(ch);
        if (!(std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.')) {
            return false;
        }
    }

    return true;
}

std::string lua_trim_ascii_whitespace(const std::string& value)
{
    std::size_t begin = 0;
    std::size_t end = value.size();

    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(begin, end - begin);
}

} // namespace olnk
