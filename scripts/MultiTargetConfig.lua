-- scripts/multi_target.lua
local target = os.getenv("TARGET") or "linux"

local configs = {
    linux = {
        arch = "x86_64",
        format = "elf",
        base = 0x400000
    },
    macos = {
        arch = "arm64",
        format = "macho",
        base = 0x100000000
    },
    windows = {
        arch = "x86_64",
        format = "pe",
        base = 0x140000000
    }
}

local cfg = configs[target]

machine {
    arch = cfg.arch,
    format = cfg.format
}

memory {
    code = { base = cfg.base, size = 0x100000, flags = "rx" }
}

sections {
    [".text"] = {
        memory = "code",
        align = 16,
        input = { "*.o(.text*)" }
    }
}
