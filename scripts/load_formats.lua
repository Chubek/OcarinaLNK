-- Deterministic format loader script scaffold.
local names = {"ELF", "PE", "Mach-O"}
for _, n in ipairs(names) do
  print("format:" .. n)
end
