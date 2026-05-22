// LLM / maintainer hints:
// - Keep traversal helpers independent from public ABI exposure details.
// - Preserve stable visitation order for deterministic passes.
// - Avoid hidden mutation during read-only traversal helpers.
// - This module intentionally operates on internal IR scaffolds only; bridge
//   to public IR ABI should remain in ir.cpp adapters.

#include "internal/ir_model.h"

#include <cstddef>
#include <functional>

namespace olnk {

class IrVisitor {
public:
    using SectionCallback = std::function<void(const IrSectionNode&)>;
    using SymbolCallback = std::function<void(const IrSymbolNode&)>;

    static bool visit_sections(const IrGraph& graph, const SectionCallback& callback)
    {
        if (!callback) {
            return false;
        }

        for (std::size_t i = 0; i < graph.sections.size(); ++i) {
            callback(graph.sections[i]);
        }

        return true;
    }

    static bool visit_symbols(const IrGraph& graph, const SymbolCallback& callback)
    {
        if (!callback) {
            return false;
        }

        for (std::size_t i = 0; i < graph.symbols.size(); ++i) {
            callback(graph.symbols[i]);
        }

        return true;
    }
};

} // namespace olnk
