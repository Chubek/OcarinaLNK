// LLM / maintainer hints:
// - Centralize diagnostics formatting/routing here.
// - Never let logging exceptions escape across ABI paths.
// - Keep default output concise and deterministic.
// - Public ABI callback wiring lives elsewhere; this file provides internal,
//   dependency-light formatting and buffering primitives.

#include <olnk/olnk-api.h>

#include <string>
#include <utility>
#include <vector>

namespace olnk {

struct DiagnosticMessage {
    olnk_diagnostic_severity_t severity = OLNK_DIAGNOSTIC_NOTE;
    std::string text;
};

class DiagnosticBuffer {
public:
    void push(olnk_diagnostic_severity_t severity, std::string message)
    {
        DiagnosticMessage entry;
        entry.severity = severity;
        entry.text = std::move(message);
        messages_.push_back(std::move(entry));
    }

    const std::vector<DiagnosticMessage>& messages() const { return messages_; }

private:
    std::vector<DiagnosticMessage> messages_;
};

const char* severity_to_string(olnk_diagnostic_severity_t severity)
{
    switch (severity) {
    case OLNK_DIAGNOSTIC_NOTE:
        return "note";
    case OLNK_DIAGNOSTIC_WARNING:
        return "warning";
    case OLNK_DIAGNOSTIC_ERROR:
        return "error";
    case OLNK_DIAGNOSTIC_FATAL:
        return "fatal";
    default:
        return "unknown";
    }
}

std::string format_diagnostic(olnk_diagnostic_severity_t severity,
                              const std::string& message)
{
    std::string output;
    output.reserve(16 + message.size());
    output += '[';
    output += severity_to_string(severity);
    output += "] ";
    output += message;
    return output;
}

} // namespace olnk
