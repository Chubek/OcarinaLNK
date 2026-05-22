local olnk = olnk or {}
return olnk.machine.define {
  name = "WASM",
  description = "WASM32 conservative machine description",
  arch = "wasm32",
  endianness = "little",
  image_class = 32,
  aliases = {"wasm", "wasm32"},
  default_page_alignment = 0x10000,
  default_section_alignment = 0x10,
  supports_relocations = true,
  relocation_map = {
    func_index_leb = "R_WASM_FUNCTION_INDEX_LEB",
    memory_addr_leb = "R_WASM_MEMORY_ADDR_LEB"
  }
}
