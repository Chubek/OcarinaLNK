local olnk = olnk or {}
return olnk.format.define {
  name = "PE",
  description = "Portable Executable baseline",
  vendor = "olnk",
  license = "MIT",
  kind = "pe",
  version = "1.0.0",
  aliases = {"pe", "coff", "pe-coff"},
  capabilities = {"executable", "shared", "debug", "imports", "relocatable"},
  defaults = {
    text = ".text",
    data = ".data",
    bss = ".bss",
    rodata = ".rdata",
    tls = ".tls"
  }
}
