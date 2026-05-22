local olnk = olnk or {}
return olnk.machine.define {
  name = "x86-64",
  description = "AMD64 System V style baseline machine description",
  arch = "x86_64",
  endianness = "little",
  image_class = 64,
  aliases = {"x86_64", "amd64", "x64"},
  default_page_alignment = 0x1000,
  default_section_alignment = 0x10,
  supports_relocations = true,
  relocation_map = {
    abs64 = "R_X86_64_64",
    pc32 = "R_X86_64_PC32",
    plt32 = "R_X86_64_PLT32"
  }
}
