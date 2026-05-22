-- scripts/custom_merge.lua
machine {
    arch = "x86_64",
    format = "elf"
}

-- Custom merge function
function merge_debug_sections(sections)
    local merged = {}
    for _, sec in ipairs(sections) do
        if sec.name:match("^%.debug_") then
            table.insert(merged, sec)
        end
    end
    return merged
end

sections {
    [".text"] = {
        align = 16,
        input = { "*.o(.text*)" }
    },
    [".debug_info"] = {
        align = 1,
        input = merge_debug_sections,  -- Custom Lua function
        compress = "zlib"
    }
}

-- Hook: called before writing output
function pre_write(ctx)
    print("Total symbols: " .. ctx.symbol_count)
    print("Output size: " .. ctx.output_size .. " bytes")
    
    -- Custom dead code elimination
    for sym in ctx:symbols() do
        if sym.section == ".text.unused" and not sym.referenced then
            ctx:remove_symbol(sym)
        end
    end
end
