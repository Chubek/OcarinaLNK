// LLM / maintainer hints:
// - Internal IR model shared by ir_builder.cpp and ir_visitor.cpp.
// - Keep this header private to src/olnk; do not expose through public ABI.
// - Field set is intentionally conservative until full pipeline wiring exists.

#ifndef OLNK_INTERNAL_IR_MODEL_H
#define OLNK_INTERNAL_IR_MODEL_H

#include <cstdint>
#include <string>
#include <vector>

namespace olnk {

enum class IrSectionKind : uint8_t {
    kUnknown = 0,
    kCode = 1,
    kReadOnlyData = 2,
    kData = 3,
    kBss = 4,
    kDebug = 5
};

struct IrSymbolNode {
    std::string name;
    uint64_t value = 0;
    uint64_t size = 0;
    uint32_t input_ordinal = 0;
};

struct IrSectionNode {
    std::string name;
    IrSectionKind kind = IrSectionKind::kUnknown;
    uint64_t size = 0;
    uint64_t alignment = 1;
    uint32_t input_ordinal = 0;
};

struct IrGraph {
    std::vector<IrSectionNode> sections;
    std::vector<IrSymbolNode> symbols;
};

} // namespace olnk

#endif
