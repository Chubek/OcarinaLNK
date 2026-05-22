-- scripts/arm_cortex_m_embedded.lua
machine {
    arch = "arm",
    subarch = "cortex-m4",
    format = "elf",
    endian = "little"
}

memory {
    flash = { base = 0x08000000, size = 512 * 1024, flags = "rx" },
    ram   = { base = 0x20000000, size = 128 * 1024, flags = "rw" }
}

sections {
    [".vectors"] = {
        memory = "flash",
        align = 4,
        input = { "startup.o(.vectors)" },
        keep = true  -- Never strip interrupt vectors
    },
    [".text"] = {
        memory = "flash",
        align = 4,
        input = { "*.o(.text)", "*.o(.text.*)", "*.o(.glue_7*)" }
    },
    [".rodata"] = {
        memory = "flash",
        align = 4,
        input = { "*.o(.rodata*)" }
    },
    [".data"] = {
        memory = "ram",
        load_memory = "flash",  -- Load from flash, run in RAM
        align = 4,
        input = { "*.o(.data*)" }
    },
    [".bss"] = {
        memory = "ram",
        align = 4,
        input = { "*.o(.bss*)", "*.o(COMMON)" }
    },
    [".stack"] = {
        memory = "ram",
        size = 8 * 1024,
        align = 8
    }
}

entry_point = "Reset_Handler"

-- Export symbols for startup code
symbols {
    __data_start__ = sections[".data"].start,
    __data_end__   = sections[".data"].end,
    __data_load__  = sections[".data"].load_start,
    __bss_start__  = sections[".bss"].start,
    __bss_end__    = sections[".bss"].end,
    __stack_top__  = sections[".stack"].end
}
