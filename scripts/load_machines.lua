-- Deterministic machine loader script scaffold.
local names = {"x86-64", "Aarch64", "WASM"}
for _, n in ipairs(names) do
  print("machine:" .. n)
end
