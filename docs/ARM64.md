# The ARM64 subset

The toolchain targets a small subset of ARM64: 14 instructions
plus conditional branches, 64-bit registers, and one data directive. The
instruction table lives in `src/arm64/isa.cc` and is the single source of
truth for the assembler's encoder and the emulator's decoder.

## Registers

| Name | Notes |
|---|---|
| `x0` … `x30` | 64-bit general purpose. By convention `x29` is the frame pointer and `x30` the link register (`blr` writes the return address there). |
| `sp` | Stack pointer. Encoded as register number 31 **in the `d` and `n` slots**. |
| `xzr` | Always zero. Encoded as register number 31 **in the `m` slot**. |
| `pc` | Program counter; not directly addressable. |
| `NZCV` | Condition flags, set only by `cmp`. |

So `add sp, sp, x1` and `add x0, x1, xzr` are legal; `add xzr, ...` and
`add x0, x1, sp` are not, and the assembler rejects them.

## Instructions

Every instruction is one 32-bit halfword. In the encodings below `d`, `n`,
`m` are 5-bit register numbers; immediates are two's complement.

| Assembly | Meaning | Encoding (big-endian bits) |
|---|---|---|
| `add xd, xn, xm` | xd = xn + xm | `10001011 001 mmmmm 011000 nnnnn ddddd` |
| `sub xd, xn, xm` | xd = xn − xm | `11001011 001 mmmmm 011000 nnnnn ddddd` |
| `mul xd, xn, xm` | xd = low 64 bits of xn × xm | `10011011 000 mmmmm 011111 nnnnn ddddd` |
| `smulh xd, xn, xm` | xd = high 64 bits of signed xn × xm | `10011011 010 mmmmm 011111 nnnnn ddddd` |
| `umulh xd, xn, xm` | xd = high 64 bits of unsigned xn × xm | `10011011 110 mmmmm 011111 nnnnn ddddd` |
| `sdiv xd, xn, xm` | xd = xn ÷ xm, signed (÷0 gives 0) | `10011010 110 mmmmm 000011 nnnnn ddddd` |
| `udiv xd, xn, xm` | xd = xn ÷ xm, unsigned (÷0 gives 0) | `10011010 110 mmmmm 000010 nnnnn ddddd` |
| `cmp xn, xm` | set NZCV from xn − xm | `11101011 001 mmmmm 011000 nnnnn 11111` |
| `br xn` | pc = xn | `11010110 000 11111 000000 nnnnn 00000` |
| `blr xn` | x30 = pc + 4; pc = xn | `11010110 001 11111 000000 nnnnn 00000` |
| `ldur xd, [xn, i]` | xd = MEM[xn + i], i ∈ [−256, 255] | `11111000 010 iiiiiiiii 00 nnnnn ddddd` |
| `stur xd, [xn, i]` | MEM[xn + i] = xd | `11111000 000 iiiiiiiii 00 nnnnn ddddd` |
| `ldr xd, i` | xd = MEM[pc + i], i a multiple of 4 in ±1 MiB | `01011000 iiiiiiiiiiiiiiiiiii ddddd` (i/4) |
| `b i` | pc += i, i a multiple of 4 in ±128 MiB | `000101 iiiiiiiiiiiiiiiiiiiiiiiiii` (i/4) |
| `b.cond i` | if cond: pc += i (±1 MiB) | `01010100 iiiiiiiiiiiiiiiiiii 0 cccc` (i/4) |

Memory operands are 8-byte words; `ldr`/`ldur`/`stur` addresses must be
4-byte aligned. Immediates in assembly are written in **bytes**; the encoder
divides PC-relative ones by 4.

### Condition codes

| Suffix | Code | Branch if | Use after `cmp a, b` |
|---|---|---|---|
| `eq` | 0000 | Z | a == b |
| `ne` | 0001 | !Z | a != b |
| `hs` | 0010 | C | a ≥ b unsigned |
| `lo` | 0011 | !C | a < b unsigned |
| `hi` | 1000 | C && !Z | a > b unsigned |
| `ls` | 1001 | !(C && !Z) | a ≤ b unsigned |
| `ge` | 1010 | N == V | a ≥ b signed |
| `lt` | 1011 | N != V | a < b signed |
| `gt` | 1100 | !Z && N == V | a > b signed |
| `le` | 1101 | !(!Z && N == V) | a ≤ b signed |

## Assembly syntax

```
label:  instruction            // comment
data:   .8byte 0x123           // one 8-byte little-endian word
        .8byte label           // the label's address (relocatable)
        .import name           // name is defined in another object file
        .export name           // make label `name` visible to other files
```

Any number of labels may precede an instruction; labels alone on a line
name the next thing emitted. Integers are decimal (optionally negative) or
`0x` hexadecimal. Branch and `ldr` targets may be labels, which the
assembler converts to PC-relative offsets.

The **load-and-skip** idiom puts a 64-bit constant in a register:

```
ldr x1, 8        // x1 = the word 8 bytes ahead
b 12             // jump over it
.8byte 1234567890123
```

## The machine model (emulator)

`bin/emu` follows the reference emulator's conventions so that reference
transcripts apply unchanged:

| Item | Value |
|---|---|
| Memory | 16 MiB (`0x1000000`), byte addressed, program loaded at 0 |
| Initial `sp`, `x29` | `0x1000000` (stack grows down from the top of memory) |
| Initial `x30` | `0xfffffe4` - a sentinel; when `pc` reaches it the program has returned to the loader |
| Arguments | `emu prog.bin 5 7` sets `x0 = 5`, `x1 = 7`; `emu -a prog.bin 1 2 3` stores the words immediately after the program and sets `x0` = their address, `x1 = 3` |
| stdin | a load (`ldur`) from `0xc000000000010000` returns the next byte, or −1 at end of file |
| stdout | a store (`stur`) to `0xc000000000010008` writes the low byte |
| Exit | prints all registers, `pc`, the flags (`vCzN`: capitals set) and `Program exited normally.` on stderr |

Fatal conditions - unaligned or out-of-range memory access, executing a
word that is not an instruction - stop the program with an `ERROR` message
and exit status 1. NCCL's `NULL` is −65536, which is out of range, so a
NULL dereference crashes as the language requires.

`emu --trace` prints every executed instruction; `bin/disasm` prints a
binary or ARMCOM file as assembly.
