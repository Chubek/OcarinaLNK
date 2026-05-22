// LLM / maintainer hints:
// - Internal IR mutation/building belongs here, not in public ABI adapters.
// - Do not invent public builder APIs beyond declared headers.
// - Keep ownership and lifetime boundaries explicit for IR nodes.
// - Public headers currently expose IR mostly as read-only views; this file
//   keeps a conservative internal model that can later feed those adapters.

#include "internal/ir_model.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace olnk {

class IrBuilder {
public:
    void add_section(IrSectionNode section)
    {
        sections_.push_back(std::move(section));
    }

    void add_symbol(IrSymbolNode symbol)
    {
        symbols_.push_back(std::move(symbol));
    }

    bool build(IrGraph* out_graph)
    {
        if (out_graph == nullptr) {
            return false;
        }

        normalize_sections();
        normalize_symbols();

        out_graph->sections = sections_;
        out_graph->symbols = symbols_;
        return true;
    }

private:
    void normalize_sections()
    {
        for (IrSectionNode& section : sections_) {
            if (section.alignment == 0) {
                section.alignment = 1;
            }
        }

        std::stable_sort(sections_.begin(), sections_.end(),
                         [](const IrSectionNode& lhs, const IrSectionNode& rhs) {
                             if (lhs.kind != rhs.kind) {
                                 return static_cast<uint8_t>(lhs.kind) <
                                        static_cast<uint8_t>(rhs.kind);
                             }
                             if (lhs.name != rhs.name) {
                                 return lhs.name < rhs.name;
                             }
                             return lhs.input_ordinal < rhs.input_ordinal;
                         });
    }

    void normalize_symbols()
    {
        std::stable_sort(symbols_.begin(), symbols_.end(),
                         [](const IrSymbolNode& lhs, const IrSymbolNode& rhs) {
                             if (lhs.name != rhs.name) {
                                 return lhs.name < rhs.name;
                             }
                             if (lhs.value != rhs.value) {
                                 return lhs.value < rhs.value;
                             }
                             return lhs.input_ordinal < rhs.input_ordinal;
                         });
    }

    std::vector<IrSectionNode> sections_;
    std::vector<IrSymbolNode> symbols_;
};

} // namespace olnk
