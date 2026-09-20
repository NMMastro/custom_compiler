# ARMCOM object files and linking

Raw machine code can only run at the address it was assembled for, and it
cannot refer to code in another file. ARMCOM is the toolchain's object-file
container, and it solves both problems: it wraps machine code with a footer
describing which words hold addresses (so a loader can move the code) and
which symbols the file imports and exports (so a linker can join files).

## Layout

```
offset  size   field
0       4      0x14000005  (the instruction "b 20")
4       8      endModule: total file length in bytes
12      8      endCode:   offset where the code ends and the footer starts
20      …      machine code
endCode …      footer records
```

All multi-byte fields are little-endian. Footer records are sequences of
8-byte words:

| Record | Words | Meaning |
|---|---|---|
| REL | `0x01`, addr | The word at `addr` holds an address into this file (it came from `.8byte label`). |
| ESR | `0x11`, addr, len, c₁ … c_len | External Symbol Reference: the word at `addr` must be filled with the address of the named symbol (one ASCII character per word). One record per use. |
| ESD | `0x12`, addr, len, c₁ … c_len | External Symbol Definition: this file defines the named symbol at `addr`. |

### Every address is a file offset

`addr` fields - and the *values* of relocated words - are offsets from the
start of the file, header included. A `.8byte start` where `start` is the
first instruction stores 20, not 0. In other words an ARMCOM file describes
itself as a program loaded at address 0 *with its header in place*, and
that is exactly why the cookie is the instruction `b 20`: an emulator that
jumps to byte 0 skips the header and runs code whose embedded addresses
are all correct.

Consequently, "stripping" an ARMCOM file to raw code means removing the
20-byte header **and** subtracting 20 from every relocated word. Our
`ObjectFile` struct keeps code-relative offsets internally; the conversion
happens only in `arm64/armcom.cc`.

## Assembling to ARMCOM

`bin/asm --armcom` emits:

* a REL record for every `.8byte label` whose label is defined in the file;
* an ESR record for every `.8byte name` where `name` was declared with
  `.import name` (the word itself is 0 as a placeholder);
* an ESD record for every `.export name`.

Branches (`b`, `b.cond`) and `ldr` use PC-relative offsets and need no
relocation.

## Linking

`bin/link a.com b.com …` (in `arm64/linker.cc`) places each file's code
after the previous one and, with `offset_i` the position of file *i*:

1. **Relocate.** For each REL in file *i*, add `offset_i` to the word it
   names; keep it as a REL of the merged file.
2. **Collect exports.** Shift each ESD by `offset_i`. Two files exporting
   the same name is an error.
3. **Resolve imports.** For each ESR, if the merged export table has the
   name, store its address in the word and record a REL there (it is now an
   ordinary address). Otherwise keep the ESR, shifted, so the result can be
   linked again later.

The output is another ObjectFile, so linking is associative:
`link(a, link(b, c))` produces the same bytes as `link(a, b, c)` (the test
suite checks this). `--strip` removes the container after verifying that no
imports remain.

Order matters only for layout. The runtime's `alloc.com` must be last,
because its `init` places the heap at the end of the program.

## Loading

A loader (see `examples/loader/`) reads an ARMCOM file, copies the code to
some address α, and for each REL adds `α − 20` to the named word: the word
held `label + 20`, and its new value must be `α + label`. Then it jumps to α.

## Verification

The tests compare our output byte for byte with object files and linked
binaries produced by the reference assembler and linker, round-trip the
prebuilt `print.com` and `alloc.com` through the reader and writer, and run
the relocating loader against a reference transcript.
