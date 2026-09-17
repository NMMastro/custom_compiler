# Build plan

The toolchain is built **bottom-up** — machine model first, language last —
so that every stage can be tested the moment it exists, using either
reference outputs from the official course tools (`tests/golden/`) or
tools already proven in an earlier step. **No step begins until the
previous step's test gate passes.**

Source for each step is adapted from the original assignment solutions
(`cs241-assignment solutions/`), refactored to the interfaces in
`ARCHITECTURE.md`. Behaviour is preserved; structure is not.

Legend for golden files: `[binasm]`, `[linkasm]`, `[linker]`, `[wlp4scan]`,
`[wlp4parse]`, `[wlp4type]`, `[wlp4c]` name the official course tool that
produced the reference.

---

## Step 0 — Scaffolding

**Build:** `Makefile` (C++20, `-Wall -Wextra -pedantic-errors`), directory
layout, `src/common/{error.h, token.h}`, `tests/run.sh` harness that runs
every `tests/*/run.sh` suite and reports pass/fail counts, `tests/golden/`
populated by copying reference files out of the assignment folders.

**Test gate:** `make` succeeds on an empty tool set; `make test` runs and
reports 0 failures.

---

## Step 1 — ISA table, assembly tokenizer, assembler (raw binary mode)

**Adapt from:** A1 `asm.cc` (encoding), A2 `asm.cpp` + `arm64.dfa`
(tokenizer), A3 `asm.cpp` (two-pass with labels, `b.cond`, `.8byte`).

**Build:**
- `common/dfa` — `.dfa` reader and SMM loop (from A2 `smm.cpp`).
- `arm64/isa` — instruction table replacing A3's `para_count`/`hash` maps;
  `encode()`; register-slot rules (`xzr` in `m` only, `sp` in `d`/`n` only);
  immediate range checks with two's-complement masking.
- `arm64/asm_lexer` — A2 tokenizer, **now emitting `NEWLINE` tokens** so its
  output feeds the assembler directly (the original relied on a course
  `tokenize` binary for this).
- `arm64/assembler` — produces an `ObjectFile`; this step only uses `.code`.
- Tools: `asm` (text in, raw binary out; `--tokens` to dump tokens), `dfa`,
  `smm`.

**Test gate — `tests/asm/`:**
| Test | Reference |
|---|---|
| `tac.asm`, `increasing.asm`, `print.asm`, `stirling.asm` → byte-identical `.bin` | `[binasm]` A3 |
| `.8byte`-only programs (`br30`, `add`, `mult`, `2x-1`, `self-modifying`) → `.arm` | `[binasm]` A1 |
| Every instruction form once, incl. all ten `b.cond`, negative immediates, `xzr`/`sp` | hand-computed from the ISA reference sheet |
| Error cases: bad register, `xzr` in `d` slot, immediate out of range, undefined label, duplicate label, label after instruction, unaligned branch | expect `ERROR`, exit 1, empty stdout |
| Tokenizer: `A3/compare.tokenized`-style dumps | `[course tokenize]` A3 tests |

---

## Step 2 — Emulator

**Build:** `arm64/emulator` using `isa::decode()`; tool `emu` with the
course's CLI (`emu prog.bin [x0 [x1]]`, `emu -a prog.bin v1 v2 …`), the same
register-dump format, and memory-mapped stdin/stdout.

**Test gate — `tests/emu/`.** Everything here runs binaries produced by
**official** tools, so the emulator is validated independently of our own
assembler and compiler:
| Test | Reference |
|---|---|
| `A1/*.arm` with the exact register dumps printed in the A1 spec (`add 30 25` → `x5=0x37`, `mult`, `2x-1 20` → `x5=0x27`, `self-modifying 5 4094` → `x3=0xd61f03c08b3f6025`, `x5=0xffe`) | A1 spec |
| `A3/tac.bin` with piped input → reversed output | A3 spec |
| `A3/print.bin` with `241`, `0`, `-1`, `INT64_MIN` → decimal text | A3 spec |
| `A3/stirling.bin` `(6,3)` → `x0=225`, other registers preserved | A3 spec table |
| `A4/practice.bin` `[wlp4c]` with the three arrays in the A4 spec → `3`, `2`, `-1` | A4 spec |
| `A4/radix.bin` `[wlp4c]` with `(12345,10)`, `(683248722,36)`, `(9172063,29)`, `(-1,3)`, `(15,1)` | A4 spec |
| Round-trip: `asm` output from Step 1 runs identically to the golden `.bin` | Step 1 |

---

## Step 3 — ARMCOM object files and the linker

**Build:**
- `arm64/assembler`: `.import`, `.export`, REL generation for `.8byte label`.
- `arm64/armcom`: writer, reader, `strip()`.
- `arm64/linker` and tool `link` (`link a.com b.com … -o out.com [--strip]`).
- Tool `asm --armcom`.

**Test gate — `tests/link/`:**
| Test | Reference |
|---|---|
| `A7/input.asm` → byte-identical `input.com`; `A8/input.asm` → `input.com`; `A7/p6.asm` → `p6.com` | `[linkasm]` |
| `link A8/output.com runtime/print.com --strip` → byte-identical `A8/linked.bin` | `[linker]` |
| `armcom` reader round-trips `print.com` and `alloc.com` (parse → write → identical bytes) | course libraries |
| Link a hand-written program against `alloc.com`, run on `emu`: `init`, `new`, write, read back, `delete` | Step 2 emulator |
| Errors: undefined import, duplicate export, `.8byte` of a label that is neither local nor imported | expect `ERROR` |
| **Loader demo:** assemble `examples/loader/load.asm`, feed `address.bin + input.com`, compare stdout with the A8 P7 spec transcript (relocation of `changeMe` visible) | A8 spec |

---

## Step 4 — WLP4 lexer

**Adapt from:** A4 `wlp4scan.cc` + `wlp4scan.dfa` (DFA becomes a raw string).

**Build:** `wlp4/lexer`, tool `wlp4scan`.

**Test gate — `tests/scan/`:**
| Test | Reference |
|---|---|
| `A4/bigtest.wlp4` → `expect.txt` (diff `-Zb`) | `[wlp4scan]` |
| `A5/smt.txt`-style small programs, every token kind at least once | `[wlp4scan]` / spec |
| Comments, tabs, keywords vs. identifiers (`Long`, `longer`, `wain2`), `==` vs `= =`, `0` vs `007` | spec |
| NUM overflow (`9223372036854775808`), illegal chars (`#`, `!` alone) → `ERROR` | spec |

---

## Step 5 — Parser

**Adapt from:** A5 `slr.cc` / `wlp4parse.cc`, tables from `wlp4data.h`.

**Build:** `common/parse_tree` (with `.wlp4i` reader/writer), `wlp4/slr_parser`,
`wlp4/grammar.h`, tool `wlp4parse`.

**Test gate — `tests/parse/`:**
| Test | Reference |
|---|---|
| `A5/smt.txt` → `expected.wlp4i` | `[wlp4parse]` |
| `A6/test.wlp4i`, `A7/test.wlp4i` regenerated from their `.wlp4` sources via `wlp4scan \| wlp4parse` | `[wlp4parse]` |
| A program exercising every grammar rule (procedures, params, all statements, all tests, all factors) | self-consistency: `parse_tree` reader ∘ writer is identity |
| Syntax errors at known token positions → exactly `ERROR at k` | A5 spec semantics |

---

## Step 6 — Type checker

**Adapt from:** A6 `wlp4type.cc`.

**Build:** `wlp4/type_checker`, tool `wlp4type`, `.wlp4ti` writer.

**Test gate — `tests/type/`:**
| Test | Reference |
|---|---|
| `A7/test.wlp4i` → `test.wlp4ti` | `[wlp4type]` |
| A6 spec example (`return 241`) → annotated output in the spec | A6 spec |
| One **negative** case per semantic rule in the WLP4 spec (~25 files): duplicate params, `wain`'s second param not `long`, use-before-declare, call-before-declare, variable shadowing a procedure then calling it, `*long`, `&(long*)`, `new long[ptr]`, `ptr*ptr`, `long - long*`, `ptr == long`, `println(ptr)`, `delete [] long`, arity/type mismatch in calls, `long* x = 0`, … | spec |
| Positive edge cases: parameter named the same as its procedure, recursion, `long p(long p)` example from spec | spec |

---

## Step 7 — Code generator and `wlp4c` driver

**Adapt from:** A8 `wlp4gen.cc`.

**Build:** `wlp4/codegen` with an `Emitter` (labels, push/pop, load-constant,
annotated output); tool `wlp4gen`; driver `wlp4c` chaining all stages in
memory with `--emit-*` flags and an optional `--link`/`--run` that invokes
the back-end libraries directly.

**Test gate — `tests/e2e/`.** Each case is a `.wlp4` file with `.args`
(register or array inputs), optional `.stdin`, and expected `.stdout` +
`.retval`. Runs `wlp4c → asm --armcom → link print.com alloc.com --strip → emu`.
| Test | Reference |
|---|---|
| `practice.wlp4` and `radix.wlp4` on the same inputs as Step 2 — **differential test**: our compiled output must match the `[wlp4c]` binaries' output | Step 2 |
| The example program from every A7/A8 problem statement (P1–P5 each), with the return values the specs state | A7/A8 specs |
| New cases: recursion (factorial, fib), pointer-walk over an array, `new` failure returns NULL, `delete [] NULL` is a no-op, NULL deref crashes, pointer comparison with large offsets (`a + 536870912`), nested if/while, `getchar` loop echo, procedures with 0/1/many params, procedure and variable with the same name | spec semantics |
| Intermediate dumps from `wlp4c --emit-*` are identical to running the standalone tools in a pipe | self-consistency |

---

## Step 8 — Examples, documentation, polish

**Build:**
- `examples/wlp4/`: the showcase programs from Step 7 with a `make examples`
  target that compiles and runs each and prints a transcript.
- `examples/asm/` and `examples/loader/` with short READMEs.
- `docs/WLP4.md`, `ARM64.md`, `ARMCOM.md`, `CODEGEN.md` — written from the
  code as built, with the register map, frame layout, and format diagrams.
- `README.md`: what it is, the pipeline diagram, a 60-second quickstart, a
  sample session showing `--emit-asm` output, and a map of the repo.
- Doc-comments on every public function; a header comment on every file
  stating its role in the pipeline.

**Test gate:** `make clean && make && make test` passes from a fresh clone;
every command in the README quickstart runs as written.

---

## Out of scope for v1

- Optimization passes (the A8 bonus). Noted as future work in the README.
- Maximal-munch backtracking (the lexer uses Simplified Maximal Munch, as the
  WLP4 spec requires).
- Any instructions outside the course's ARM64 subset.
