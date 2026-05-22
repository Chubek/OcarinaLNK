# OcarinaLNK Examples

These examples exercise conservative, deterministic `olnk` workflows.

## 1) Minimal ELF link

- Path: `examples/minimal-elf`
- Command:

```bash
examples/minimal-elf/run.sh
```

## 2) PE + WASM alias resolution

- Path: `examples/pe-wasm`
- Uses aliases from `formats/PE.lua` and `machines/WASM.lua`.
- Command:

```bash
examples/pe-wasm/run.sh
```

## 3) Profile-driven CLI invocation

- Path: `examples/profile-driven`
- Uses `--profile` and `--profile-format json`.
- Command:

```bash
examples/profile-driven/run.sh
```

## Notes

- Each script compiles tiny C files into `.o` inputs with `cc`.
- Final linking/output emission is performed by `olnk`.
- Override binary path with `OLNK=/path/to/olnk`.
