# Build plan

> **Status:** all eight steps are complete and every gate passes -
> `make test` reports 251 checks across 7 suites. This document is kept as a
> record of how the project was built and verified.

The toolchain was built **bottom-up** - machine model first, language last -
so that every stage could be tested the moment it existed, using either
reference outputs (`tests/golden/`, produced by a pre-existing reference
implementation of the toolchain) or tools already proven in an earlier
step. **No step began until the previous step's test gate passed.**

Each step started from earlier, standalone implementations of the same
stage (see the README's *Origins* section) and refactored them to the
interfaces in `ARCHITECTURE.md`. Behaviour was preserved; structure was not.

---

## Step 0 - Scaffolding

**Build:** `Makefile` (C++20, `-Wall -Wextra -pedantic-errors`), directory
layout, `src/common/{error.h, token.h}`, `tests/run.sh` harness that runs
every `tests/*/run.sh` suite and reports pass/fail counts, `tests/golden/`
populated with the reference outputs.

**Test gate:** `make` succeeds on an empty tool set; `make test` runs and
reports 0 failures.

---

## Step 1 - ISA table, assembly tokenizer, assembler (raw binary mode)

**Build:**
- `common/dfa` - `.dfa` reader and Simplified Maximal Munch.
- `arm64/isa` - the instruction table; `encode()`; register-slot rules
  (`xzr` in `m` only, `sp` in `d`/`n` only); immediate range checks with
  two's-complement masking.
- `arm64/asm_lexer` - assembly tokenizer emitting `NEWLINE` tokens so its
  output feeds the assembler directly.
- `arm64/assembler` - produces an `ObjectFile`; this step only uses `.code`.
- Tools: `asm` (text in, raw binary out; `--tokens` to dump tokens), `dfa`,
  `smm`.

**Test gate - `tests/asm/`:**
| Test | Reference |
|---|---|
| `tac.asm`, `increasing.asm`, `print.asm`, `stirling.asm` → byte-identical `.bin` | reference assembler |
| `.8byte`-only programs (`br30`, `add`, `mult`, `2x-1`, `self-modifying`) → `.arm` | reference assembler |
| Every instruction form once, incl. all ten `b.cond`, negative immediates, `xzr`/`sp` | independent Python encoder written from the bit patterns in `ARM64.md` |
| Error cases: bad register, `xzr` in `d` slot, immediate out of range, undefined label, duplicate label, label after instruction, unaligned branch | expect `ERROR`, exit 1, empty stdout |
| Tokenizer output for a line covering every token kind | hand-written expectation |

---

## Step 2 - Emulator

**Build:** `arm64/emulator` using `isa::decode()`; tool `emu` with the
reference CLI (`emu prog.bin [x0 [x1]]`, `emu -a prog.bin v1 v2 …`), the
same register-dump format, and memory-mapped stdin/stdout.

**Test gate - `tests/emu/`.** Everything here runs binaries produced by the
**reference** toolchain, so the emulator is validated independently of our
own assembler and compiler:
| Test | Reference |
|---|---|
| Machine-code programs with exact register dumps (`add 30 25` → `x5=0x37`, `2x-1 20` → `x5=0x27`, `self-modifying 5 4094` → `x3=0xd61f03c08b3f6025`, `x5=0xffe`) | reference transcripts |
| `tac.bin` with piped input → reversed output | reference transcript |
| `print.bin` with `241`, `0`, `-1`, `INT64_MIN` → decimal text | reference transcript |
| `stirling.bin` `(6,3)` → `x0=225`, other registers preserved | reference table |
| `practice.bin` (reference compiler) with three arrays → `3`, `2`, `-1` | reference transcript |
| `radix.bin` (reference compiler) with `(12345,10)`, `(683248722,36)`, `(571408,29)`, `(-1,3)`, `(15,1)` | reference transcript |
| Round-trip: `asm` output from Step 1 runs identically to the golden `.bin` | Step 1 |

---

## Step 3 - ARMCOM object files and the linker

**Build:**
- `arm64/assembler`: `.import`, `.export`, REL generation for `.8byte label`.
- `arm64/armcom`: writer, reader, `strip()`.
- `arm64/linker` and tool `link` (`link a.com b.com … [--strip]`).
- Tool `asm --armcom`.

**Test gate - `tests/link/`:**
| Test | Reference |
|---|---|
| `literal.asm`, `selfpatch.asm` → byte-identical `.com`; `print_once.asm` → byte-identical raw binary | reference assembler |
| `link ptr_compare.com runtime/print.com --strip` → byte-identical `ptr_compare_linked.bin` | reference linker |
| `armcom` reader round-trips `print.com` and `alloc.com` (parse → write → identical bytes) | runtime libraries |
| Link a hand-written program against `alloc.com`, run on `emu`: `init`, `new`, write, read back, `delete` | Step 2 emulator |
| Three-file link with chained imports; `link(a, link(b, c)) == link(a, b, c)` | self-consistency |
| Errors: undefined import, duplicate export, `.8byte` of a label that is neither local nor imported | expect `ERROR` |
| **Loader demo:** assemble `examples/loader/load.asm`, feed `alpha + prog.com`, compare stdout with the reference transcript (relocation of `changeMe` visible) | reference transcript |

---

## Step 4 - NCCL lexer

**Build:** `nccl/lexer` over `nccl_dfa.h` (the DFA as a raw string), tool
`ncclscan`.

**Test gate - `tests/scan/`:**
| Test | Reference |
|---|---|
| `bigtest.nccl` → `bigtest.tokens` (diff `-b`) | reference scanner |
| A file with every token kind, keywords vs. identifiers (`Long`, `longer`, `wain2`), `==` vs `= =`, `0` vs `007`, comments and tabs | hand-written expectation |
| NUM overflow (`9223372036854775808`), illegal chars (`#`, `!` alone, `_`, `"`) → `ERROR` | language definition |

---

## Step 5 - Parser

**Build:** `common/parse_tree` (with `.tree`/`.typed` reader and writer),
`nccl/slr_parser`, `nccl/grammar.h`, tool `ncclparse`.

**Test gate - `tests/parse/`:**
| Test | Reference |
|---|---|
| `minimal.tokens` → `minimal.tree`; `calls.nccl` → `calls.tree`; `println.nccl` → the untyped `println.typed` | reference parser |
| `allrules.nccl`, a program exercising every grammar rule - every production appears in the output | grammar in `grammar.h` |
| Tree text round trip (`ncclparse --reparse`) is the identity | self-consistency |
| Syntax errors at known token positions → exactly `ERROR at k` | hand-counted |

---

## Step 6 - Type checker

**Build:** `nccl/type_checker`, tool `nccltype`.

**Test gate - `tests/type/`:**
| Test | Reference |
|---|---|
| `println.nccl` → `println.typed` | reference type checker |
| `minimal.typed` (`return 241`) → annotated output | hand-written expectation |
| One **negative** program per semantic rule in `NCCL.md` (42 files): duplicate params, `wain`'s second param not `long`, use-before-declare, call-before-declare, variable shadowing a procedure then calling it, `*long`, `&(long*)`, `new long[ptr]`, `ptr*ptr`, `long - long*`, `ptr == long`, `println(ptr)`, `delete [] long`, arity/type mismatch in calls, `long* x = 0`, … | language definition |
| Positive edge cases: parameter named the same as its procedure, recursion, same local names in different procedures, every pointer-arithmetic form | language definition |

---

## Step 7 - Code generator and `ncclc` driver

**Build:** `nccl/codegen` with an `Emitter` (labels, push/pop, load-constant,
annotated output); tool `ncclgen`; driver `ncclc` chaining all stages in
memory with `--emit-*` flags and `--link`/`--run` that invoke the back-end
libraries directly.

**Test gate - `tests/e2e/`.** Each case is a directory with `prog.nccl`,
`args` (register or array inputs), optional `stdin`, and expected `stdout`
plus `retval` (or a `crash` marker). Runs
`ncclc → asm --armcom → link print.com alloc.com --strip → emu`.
| Test | Reference |
|---|---|
| `practice.nccl` and `radix.nccl` on the same inputs as Step 2 - **differential test**: our compiled output must match the reference compiler's binaries | Step 2 |
| Twelve small programs, one per language feature in the order it was added (return a parameter, declarations, binary operators, control flow, I/O, calls, pointers, pointer arithmetic, pointer comparison, `new`/`delete`) | hand-computed results |
| Recursion (factorial, fib), pointer-walk over an array, `new` failure returns NULL, `delete [] NULL` is a no-op, NULL deref crashes, pointer comparison with large offsets (`a + 536870912`), nested if/while, `getchar` loop echo, procedures with 0/1/many params, procedure and variable with the same name, 64-bit extremes | language definition |
| Intermediate dumps from `ncclc --emit-*` are byte-identical to running the standalone tools in a pipe | self-consistency |

---

## Step 8 - Examples, documentation, polish

**Build:**
- `examples/nccl/`: showcase programs with a `make examples` target that
  compiles and runs each and prints a transcript.
- `examples/asm/` and `examples/loader/` with short READMEs.
- `docs/NCCL.md`, `ARM64.md`, `ARMCOM.md`, `CODEGEN.md` - written from the
  code as built, with the register map, frame layout, and format diagrams.
- `README.md`: what it is, the pipeline diagram, a 60-second quickstart, a
  sample session, and a map of the repo.
- Doc-comments on every public function; a header comment on every file
  stating its role in the pipeline.

**Test gate:** `make clean && make && make test` passes from a fresh clone;
every command in the README quickstart runs as written.

---

## Out of scope for v1

- Optimization passes. Noted as future work in the README.
- Maximal-munch backtracking (the lexer uses Simplified Maximal Munch).
- Any instructions outside the ARM64 subset in `ARM64.md`.
