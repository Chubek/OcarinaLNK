local olnk = olnk or {}
return olnk.format.define {
  name = "Mach-O",
  description = "Mach object baseline",
  vendor = "olnk",
  license = "MIT",
  kind = "mach-o",
  version = "1.0.0",
  aliases = {"macho", "mach-o", "macho64"},
  capabilities = {"executable", "shared", "debug", "pic"},
  defaults = {
    text = "__TEXT,__text",
    data = "__DATA,__data",
    bss = "__DATA,__bss",
    rodata = "__TEXT,__const"
  }
}
