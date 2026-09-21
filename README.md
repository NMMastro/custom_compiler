# NCCL - Nate's Custom Coding Language

A complete compiler and systems toolchain, written in C++20, for **NCCL** - a
small C-like language with 64-bit integers, pointers, procedures,
`new`/`delete`, and character I/O - targeting a subset of **ARM64**. Every
layer between source text and a running program is implemented here, from
the scanner down to the CPU emulator that executes the result.

```c
long fib(long n) {
  long r = 0;
  if (n < 2) { r = n; } else { r = fib(n - 1) + fib(n - 2); }
  return r;
}

long wain(long n, long unused) {
  println(fib(n));
  return 0;
}
```

```
$ bin/ncclc --run fib.nccl 20
6765
```

## The pipeline

```
source.nccl ─scan─▶ tokens ─parse─▶ parse tree ─typecheck─▶ typed tree ─codegen─▶ prog.asm ─┐
                                                                                            │
  ┌─────────────────────────────────────────────────────────────────────────────────────────┘
  │
  └─▶ asm --armcom ─▶ prog.com ──────────┐
                     runtime/print.com ──┼─▶ link --strip ─▶ prog.bin ─▶ emu [-a arr] ─▶ stdout, x0
                     runtime/alloc.com ──┘
```

| Stage | Tool | Technique |
|---|---|---|
| Scanner | `ncclscan` | DFA + Simplified Maximal Munch |
| Parser | `ncclparse` | Table-driven SLR(1), builds a parse tree |
| Type checker | `nccltype` | Per-procedure symbol tables, procedure signatures, the full type system |
| Code generator | `ncclgen` | Stack-machine codegen with a frame pointer; annotated output |
| Driver | `ncclc` | Runs all stages in memory; `--emit-*` any intermediate; `--link`, `--run` |
| Assembler | `asm` | Two-pass; labels; `.8byte`; ARMCOM object output with relocations, imports, exports |
| Linker | `link` | Merges ARMCOM files: relocation, symbol resolution, duplicate/undefined detection |
| Emulator | `emu` | ARM64-subset CPU with memory-mapped I/O; register dumps and instruction traces |
| Disassembler | `disasm` | Decodes machine code or ARMCOM files |
| Loader | `examples/loader` | Relocating loader written in ARM64 assembly |

## Quick start

```sh
make                      # builds every tool into bin/   (needs a C++20 compiler)
make test                 # runs 250+ checks across seven suites
make examples             # compiles and runs the programs in examples/

# Compile and run a program
cat > hello.nccl <<'EOF'
long wain(long a, long b) {
  println(a * b);
  return 0;
}
EOF
bin/ncclc --run hello.nccl 6 7        # prints 42, then the register dump on stderr

# Look at any intermediate
bin/ncclc --emit-tokens hello.nccl    # KIND lexeme lines
bin/ncclc --emit-typed hello.nccl     # the parse tree, with types
bin/ncclc hello.nccl                  # the generated assembly (annotated)

# Or run the stages as separate tools, Unix style
bin/ncclscan hello.nccl | bin/ncclparse | bin/nccltype | bin/ncclgen > hello.asm
bin/asm --armcom hello.asm > hello.com
bin/link --strip hello.com runtime/print.com runtime/alloc.com > hello.bin
bin/emu hello.bin 6 7
```

A sample session:

```
$ bin/ncclc --run examples/nccl/sieve.nccl 20
2
3
5
7
11
13
17
19
x0:  0x8            x16: 0x0
x1:  0x0            x17: 0x0
...
pc:  0xfffffe4      instr: hlt
flags: vCZn
Program exited normally.
```

## The language in one paragraph

NCCL programs are a sequence of procedures ending in `wain`, the entry point,
which takes two arguments (`long, long` or `long*, long` for an input
array). There are two types, `long` and `long*`; declarations come first in
a procedure and are initialized with constants; control flow is `if`/`else`
and `while`; expressions have the usual arithmetic, pointer arithmetic,
`&`, `*`, `new long[n]`, procedure calls, and `getchar()`; statements
include `println`, `putchar`, and `delete []`. Every valid program is also a
valid C++ fragment. The full definition - tokens, grammar, and type rules -
is in [`docs/NCCL.md`](docs/NCCL.md).

## How it is verified

Each stage is tested in isolation against reference outputs checked in under
`tests/golden/`, and the pieces are then used to validate each other:

* **Assembler** → byte-identical to the reference assembler on every
  assembly program in the repository, plus an independent Python encoder for
  every instruction form.
* **ARMCOM writer / linker** → byte-identical to the reference object files
  and linked binaries; round-trips the prebuilt `print.com` and `alloc.com`.
* **Emulator** → runs binaries produced by the *reference* compiler and
  reproduces the reference register dumps.
* **Front end** → reference parse trees and typed trees; a negative test for
  every semantic rule in the language.
* **Compiler** → a differential test: programs compiled by `ncclc` must behave
  identically to the reference compiler's binaries on the same inputs, running
  on the now-trusted emulator. Plus 36 end-to-end programs covering recursion,
  pointer arithmetic, heap exhaustion, NULL crashes, and I/O.

```
$ make test
== asm      37 passed, 0 failed
== e2e      60 passed, 0 failed
== emu      60 passed, 0 failed
== link     19 passed, 0 failed
== parse    14 passed, 0 failed
== scan      8 passed, 0 failed
== type     53 passed, 0 failed
TOTAL: 251 passed, 0 failed
```

## Repository map

```
src/common/    tokens, DFA + maximal munch, parse tree and its text formats
src/nccl/      lexer, SLR(1) parser (+ grammar tables), type checker, code generator
src/arm64/     ISA table, assembler, ARMCOM container, linker, emulator
src/tools/     one main() per command-line tool
runtime/       print.com and alloc.com (prebuilt runtime libraries)
examples/      NCCL programs, hand-written assembly, the relocating loader
tests/         one suite per stage; golden/ holds reference outputs
docs/          ARCHITECTURE · NCCL · ARM64 · ARMCOM · CODEGEN · BUILD_PLAN
```

Start with [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the design,
then [`docs/CODEGEN.md`](docs/CODEGEN.md) for what the compiler actually
emits and why.

## Design notes worth reading

* **One instruction table.** `src/arm64/isa.cc` defines each mnemonic's opcode
  bits and operand layout once; the assembler encodes with it and the emulator
  decodes with it, so the two cannot disagree.
* **ARMCOM's self-describing trick.** An object file's cookie is the
  instruction `b 20` and all of its addresses are file offsets, so the file can
  be *executed as-is* at address 0 - the header jumps over itself. See
  [`docs/ARMCOM.md`](docs/ARMCOM.md).
* **Readable output.** The code generator comments its own assembly
  (`// call twice`, `// &c`), and the DFAs that drive both scanners are kept as
  plain text so they document the lexical rules.
* **Errors are exceptions.** One `CompileError` type carries stage + message
  from any library function to the tool's `main()`; no sentinel values.

## Limitations and future work

* No optimization: every expression goes through the stack, every constant
  is a 16-byte load-and-skip. A peephole pass or constant folding would be
  the natural next step.
* The scanner is Simplified Maximal Munch, so it does not backtrack.
* Only a 15-instruction ARM64 subset is supported.

## Origins

This project grew out of CS 241 at the University of Waterloo, where a
compiler for a small language is built over a term as eight separate
assignments - machine code and an assembler first, then a scanner, an SLR(1)
parser, a type checker, and finally a code generator - each graded in
isolation against the course's own reference tools. NCCL is that language,
and this repository is my solutions to those assignments refactored into one
coherent, documented, tested toolchain: shared infrastructure instead of
copy-pasted starter code, exceptions instead of sentinel returns, a single
ISA table, and the pieces the course provided as black boxes (the emulator,
the linker, the object-file format) implemented from scratch. The prebuilt
runtime libraries `runtime/print.com` and `runtime/alloc.com`, the SLR(1)
parse tables in `src/nccl/grammar.h`, and the reference outputs under
`tests/golden/` come from the course; everything else is my own.
