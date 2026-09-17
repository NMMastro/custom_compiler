# Architecture

`wlp4` is a complete toolchain that compiles **WLP4** (a C-like language with
`long`/`long*`, procedures, `new`/`delete`, and character I/O) into runnable
**ARM64** machine code. It includes every layer of the classic pipeline:

```
 source.wlp4 ─scan─▶ tokens ─parse─▶ parse tree ─typecheck─▶ typed tree ─codegen─▶ prog.asm
                                                                                       │
                       runtime/print.com   runtime/alloc.com                     asm --armcom
                                   │              │                                    │
                                   └───────▶    link    ◀──── prog.com ◀───────────────┘
                                                 │
                                          --strip ▶ prog.bin ───▶ emu [-a arr] ▶ stdout, x0
```

Every arrow is a documented text or binary format, so each stage can be run,
inspected, and tested on its own. The `wlp4c` driver runs the first row in
memory (no re-parsing between stages) and can dump any intermediate with
`--emit-tokens`, `--emit-wlp4i`, `--emit-wlp4ti`, or `--emit-asm`.

## Design principles

1. **Library + thin tools.** All logic lives in two libraries (`wlp4::` and
   `arm64::`). Each command-line tool is a short `main()` that reads a format,
   calls one library function, and writes a format.
2. **One source of truth for the ISA.** `arm64/isa` holds the instruction table
   (mnemonic, opcode bits, operand kinds). The assembler encodes with it and the
   emulator decodes with it, so an encoding bug cannot hide in one without
   showing up in the other.
3. **Stages communicate through documented formats** — token lines, `.wlp4i`,
   `.wlp4ti`, ARM64 assembly text, ARMCOM object files, raw machine code.
4. **Errors are exceptions.** `CompileError{stage, message}` propagates to the
   tool's `main()`, which prints `ERROR: <stage>: <message>` to stderr and
   exits 1. No sentinel return values.
5. **No manual memory management.** Parse trees use `std::unique_ptr`; there is
   no `new`/`delete` outside the WLP4 programs being compiled.
6. **Generated assembly is self-describing.** The code generator emits
   `// comments` naming the source construct, so `prog.asm` reads like a
   worked example rather than a dump.

## Repository layout

```
custom_compiler/
├── README.md                   overview · quickstart · demo transcript
├── Makefile                    builds libraries + tools; `make test`; `make examples`
├── docs/
│   ├── ARCHITECTURE.md         this file
│   ├── BUILD_PLAN.md           implementation order with test gates
│   ├── WLP4.md                 language reference: tokens, grammar, type rules
│   ├── ARM64.md                instruction subset, encodings, I/O, emulator conventions
│   ├── ARMCOM.md               object-file format and the linking algorithm
│   └── CODEGEN.md              calling convention, frame layout, register map
├── src/
│   ├── common/
│   │   ├── error.h             CompileError
│   │   ├── token.h             Token{kind, lexeme}; "KIND lexeme" line I/O
│   │   ├── dfa.{h,cc}          DFA loaded from the .dfa text format; SMM scan loop
│   │   └── parse_tree.{h,cc}   Node{rule|token, children, type}; .wlp4i/.wlp4ti I/O
│   ├── wlp4/                   namespace wlp4 — the front end and code generator
│   │   ├── wlp4_dfa.h          scanner DFA as a raw-string literal
│   │   ├── lexer.{h,cc}        scan(): SMM + keyword promotion + NUM range check
│   │   ├── grammar.h           CFG + SLR(1) transition/reduction tables (raw strings)
│   │   ├── slr_parser.{h,cc}   generic SLR(1) driver → ParseTree
│   │   ├── type_checker.{h,cc} symbol tables, signatures, type rules; annotates tree
│   │   └── codegen.{h,cc}      typed tree → ARM64 assembly text (Emitter helper)
│   ├── arm64/                  namespace arm64 — the back end and machine model
│   │   ├── isa.{h,cc}          instruction table; encode() / decode()
│   │   ├── asm_dfa.h           assembly-language scanner DFA (raw string)
│   │   ├── asm_lexer.{h,cc}    assembly tokenizer (emits NEWLINE tokens)
│   │   ├── object.h            ObjectFile{code, relocations, imports, exports}
│   │   ├── assembler.{h,cc}    two-pass assembler → ObjectFile
│   │   ├── armcom.{h,cc}       ObjectFile ⇄ ARMCOM bytes; strip()
│   │   ├── linker.{h,cc}       merge ObjectFiles: relocate, resolve, check
│   │   └── emulator.{h,cc}     registers, memory, memory-mapped I/O, run loop
│   └── tools/                  one main() per executable
│       wlp4scan wlp4parse wlp4type wlp4gen wlp4c    (front end)
│       asm link emu                                  (back end)
│       dfa smm                                       (generic automata tools)
├── runtime/                    print.com, alloc.com (course-provided libraries),
│                               print.asm (source for print)
├── examples/
│   ├── wlp4/                   showcase WLP4 programs
│   ├── asm/                    hand-written ARM64: tac, increasing, print, stirling
│   └── loader/                 relocating ARMCOM loader + demo script
├── tests/
│   ├── run.sh                  `make test` entry point; runs every suite below
│   ├── golden/                 reference outputs produced by the official course tools
│   └── cases/                  WLP4 programs with expected stdout / return value
└── cs241-assignment solutions/ the original per-assignment code, kept as history
```

## Module responsibilities

### `common`

| Unit | Responsibility |
|---|---|
| `error.h` | `CompileError` (stage name + message). Thrown by every stage; caught only in `main()`. |
| `token.h` | `Token{kind, lexeme}` plus readers/writers for the one-token-per-line format shared by the WLP4 scanner, the assembly tokenizer, and the parser. |
| `dfa` | A deterministic finite automaton parsed from the course `.dfa` text format (`.ALPHABET / .STATES / .TRANSITIONS`). Provides `accepts(string)` and `scanSMM(string)` — the Simplified Maximal Munch loop used by both lexers. |
| `parse_tree` | `Node` with an owning `children` vector. A node is either a *rule* (`lhs rhs...`) or a *token* (`KIND lexeme`), optionally annotated with a type. Reads/writes the preorder `.wlp4i` / `.wlp4ti` formats. |

### `wlp4` (front end)

| Unit | Input → Output | Notes |
|---|---|---|
| `lexer` | source text → `vector<Token>` | SMM over `wlp4_dfa.h`; skips whitespace and `//` comments; promotes keyword IDs (`wain`, `long`, …); rejects NUMs outside 64-bit signed range. |
| `slr_parser` | tokens → `ParseTree` | Table-driven SLR(1). Tables in `grammar.h` are the course-generated automaton for the WLP4 grammar. On failure throws `ERROR at k` (k = 1 + tokens consumed). Generic over any `.CFG/.TRANSITIONS/.REDUCTIONS` triple. |
| `type_checker` | `ParseTree&` → annotated in place | One symbol table per procedure (`name → type`), a global procedure table (`name → signature`). Implements every rule in the WLP4 spec's *Context-Sensitive Syntax* section; each violation throws with the rule that failed. |
| `codegen` | typed tree → assembly text | Stack-machine style: every expression leaves its value in `x0`; binary operators push the left operand. Emits `.import print init new delete`. See `CODEGEN.md` for the frame layout and register map. |

### `arm64` (back end)

| Unit | Responsibility |
|---|---|
| `isa` | The instruction table: for each mnemonic, its fixed opcode bits, operand pattern (`d,n,m` registers / immediates and their widths), and rules such as "`xzr` only in the `m` slot, `sp` only in `d`/`n`". `encode(Instr) → uint32_t`, `decode(uint32_t) → Instr`. Also the ten `b.cond` condition codes. |
| `asm_lexer` | SMM tokenizer for assembly (`ID REG ZREG INT HEXINT DOTID LABEL COMMA LBRACK RBRACK NEWLINE`). |
| `assembler` | Pass 1 groups tokens into lines and builds the symbol table (labels → byte offsets). Pass 2 encodes each instruction, resolving labels: `b`/`b.cond`/`ldr` become PC-relative offsets; `.8byte label` becomes an absolute address **plus a REL entry**; `.import id` declares an external symbol (uses become ESR entries); `.export id` publishes a label (ESD entry). Output is an in-memory `ObjectFile`. |
| `armcom` | Serializes an `ObjectFile` to the ARMCOM container (20-byte header, code, footer of REL/ESR/ESD records) and back. `strip()` drops the container to leave raw machine code. |
| `linker` | Concatenates code segments, shifts each file's RELs and ESDs by its placement offset, patches every ESR from the merged ESD table, and reports duplicate or unresolved symbols. Output is another `ObjectFile`, so linking is associative and the result can be linked again or stripped. |
| `emulator` | 31 general registers + `sp`, NZCV flags, a flat byte-addressed memory. Loads a program at address 0, seeds `x0`/`x1` (or an array via `-a`), sets `sp` and a sentinel return address in `x30`, and runs the fetch–decode–execute loop until control returns to the sentinel. Loads from `0xc000000000010000` read stdin; stores to `0xc000000000010008` write stdout. Prints the register dump the course emulator prints. |

## Data formats

| Format | Producer → Consumer | Description |
|---|---|---|
| Token lines | `wlp4scan` → `wlp4parse`; `asm_lexer` → `assembler` | `KIND lexeme\n` per token. |
| `.wlp4i` | `wlp4parse` → `wlp4type` | Preorder traversal of the parse tree; rule nodes print `lhs rhs…`, leaves print `KIND lexeme`. |
| `.wlp4ti` | `wlp4type` → `wlp4gen` | `.wlp4i` with ` : long` / ` : long*` appended to expression nodes. |
| ARM64 assembly | `wlp4gen` → `asm` | Text; instruction subset in `ARM64.md`; directives `.8byte`, `.import`, `.export`. |
| ARMCOM | `asm --armcom` → `link`; `link` → `link`/loader | Object container; see `ARMCOM.md`. |
| Raw binary | `asm`, `link --strip` → `emu` | Little-endian machine code, entry at byte 0. |

## Error handling contract

Every tool exits 0 with the documented output on success, or prints a line
beginning with `ERROR` to stderr, prints nothing further to stdout, and exits 1.
Two tools have exact message requirements inherited from the course specs:
`wlp4parse` prints `ERROR at k`, and the assembler prefixes messages with
`ERROR: `. Everything else prints `ERROR: <stage>: <detail>`.

## Testing philosophy

The course tools that graded the original assignments are not available on
this machine, but their *outputs* on many inputs are, and they are checked into
`tests/golden/`. Each stage is validated against those references before the
next stage is built (see `BUILD_PLAN.md`). The emulator is validated by running
binaries produced by the *official* WLP4 compiler; the compiler is then
validated by running its own output on the now-trusted emulator. That closes
the loop with no external dependencies.
