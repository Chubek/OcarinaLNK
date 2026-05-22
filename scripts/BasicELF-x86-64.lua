-- scripts/elf_x86_64_basic.lua
machine {
    arch = "x86_64",
    format = "elf",
    endian = "little"
}

memory {
    text = { base = 0x400000, size = 0x100000, flags = "rx" },
    data = { base = 0x500000, size = 0x100000, flags = "rw" }
}

sections {
    [".text"] = {
        memory = "text",
        align = 16,
        input = { "*.o(.text)", "*.o(.text.*)" }
    },
    [".rodata"] = {
        memory = "text",
        align = 8,
        input = { "*.o(.rodata)", "*.o(.rodata.*)" }
    },
    [".data"] = {
        memory = "data",
        align = 8,
        input = { "*.o(.data)", "*.o(.data.*)" }
    },
    [".bss"] = {
        memory = "data",
        align = 8,
        input = { "*.o(.bss)", "*.o(.bss.*)" }
    }
}

entry_point = "_start"
