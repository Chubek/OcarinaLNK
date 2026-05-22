local olnk = olnk or {}
return olnk.format.define {
  name = "WASM",
  description = "WebAssembly binary format baseline",
  vendor = "olnk",
  license = "MIT",
  kind = "wasm",
  version = "1.0.0",
  aliases = {"wasm", "wasm32"},
  capabilities = {"relocatable", "imports", "debug"},
  defaults = {
    code = "code",
    data = "data"
  }
}
