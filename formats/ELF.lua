local olnk = olnk or {}
return olnk.format.define {
  name = "ELF",
  description = "Executable and Linkable Format baseline",
  vendor = "olnk",
  license = "MIT",
  kind = "elf",
  version = "1.0.0",
  aliases = {"elf", "gnu-elf", "elf64"},
  capabilities = {"executable", "shared", "relocatable", "debug", "tls", "pic"},
  defaults = {
    text = ".text",
    data = ".data",
    bss = ".bss",
    rodata = ".rodata",
    tls = ".tdata"
  }
}
