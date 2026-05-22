-- scripts/elf_shared_library.lua
machine {
    arch = "x86_64",
    format = "elf",
    type = "shared"
}

options {
    pic = true,  -- Position-independent code
    soname = "libmylib.so.1"
}

sections {
    [".text"] = {
        align = 16,
        input = { "*.o(.text*)" }
    },
    [".rodata"] = {
        align = 8,
        input = { "*.o(.rodata*)" }
    },
    [".data.rel.ro"] = {
        align = 8,
        input = { "*.o(.data.rel.ro*)" }
    },
    [".data"] = {
        align = 8,
        input = { "*.o(.data*)" }
    },
    [".bss"] = {
        align = 8,
        input = { "*.o(.bss*)" }
    },
    [".init_array"] = {
        align = 8,
        input = { "*.o(.init_array*)" },
        keep = true
    },
    [".fini_array"] = {
        align = 8,
        input = { "*.o(.fini_array*)" },
        keep = true
    }
}

-- Export version script
version_script = "exports.ver"
