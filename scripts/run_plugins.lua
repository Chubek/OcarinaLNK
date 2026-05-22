-- Conservative plugin run list scaffold.
local names = {
  "append-stub",
  "dump-symtbl",
  "dwarf-embeddings",
  "identical-code-folding",
  "linker-map-generation",
  "visualization"
}
for _, n in ipairs(names) do
  print("plugin:" .. n)
end
