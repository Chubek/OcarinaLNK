-- scripts/macho_arm64_macos.lua
machine {
    arch = "arm64",
    format = "macho",
    platform = "macos",
    min_version = "11.0"
}

sections {
    ["__TEXT,__text"] = {
        align = 16,
        flags = "code",
        input = { "*.o(__TEXT,__text)" }
    },
    ["__TEXT,__cstring"] = {
        align = 1,
        flags = "cstring_literals",
        input = { "*.o(__TEXT,__cstring)" }
    },
    ["__DATA,__data"] = {
        align = 8,
        input = { "*.o(__DATA,__data)" }
    },
    ["__DATA,__bss"] = {
        align = 8,
        input = { "*.o(__DATA,__bss)" }
    }
}

entry_point = "_main"

-- Link against system frameworks
frameworks = { "Foundation", "CoreFoundation" }
