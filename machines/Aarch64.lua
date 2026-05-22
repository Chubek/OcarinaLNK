local olnk = olnk or {}
return olnk.machine.define {
  name = "Aarch64",
  description = "AArch64 ELF baseline machine description",
  arch = "aarch64",
  endianness = "little",
  image_class = 64,
  aliases = {"arm64", "aarch64"},
  default_page_alignment = 0x1000,
  default_section_alignment = 0x10,
  supports_relocations = true,
  relocation_map = {
    abs64 = "R_AARCH64_ABS64",
    call26 = "R_AARCH64_CALL26"
  }
}
