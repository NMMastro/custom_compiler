# Code generation

`nccl/codegen.cc` turns a type-annotated parse tree into ARM64 assembly. The
strategy is the simplest correct one - a stack machine with a frame pointer
- chosen so that every construct maps to a short, recognizable instruction
sequence. There is no register allocation and no optimization.

## Register map

| Register | Role | Set by |
|---|---|---|
| `x0` | Result of every expression; return value of every procedure; argument to the runtime | everywhere |
| `x1` | Left operand of a binary operator (popped from the stack); address of an assignment target | expression code |
| `x2` | Scratch for `%` | `term PCT factor` |
| `x5` | Address of the procedure being called | calls |
| `x6` | `NULL` = −65536 | wain prologue |
| `x8` | 8, the word size (used by every push/pop) | wain prologue |
| `x10` | Address of the runtime's `print` | wain prologue |
| `x11` | Memory-mapped stdin address `0xc000000000010000` | wain prologue |
| `x12` | Memory-mapped stdout address `0xc000000000010008` | wain prologue |
| `x29` | Frame pointer | procedure entry |
| `x30` | Link register (return address) | `blr` |
| `sp` | Stack pointer | push/pop |

The constants in `x6`–`x12` are loaded once at the start of `wain`. Because
`wain` is emitted first and is the program's entry point, every other
procedure can rely on them.

## Stack idioms

```
push xN:    stur xN, [sp, -8]      pop xN:    add sp, sp, x8
            sub sp, sp, x8                    ldur xN, [sp, -8]
```

After a push the pushed word is at `[sp, 0]`. Loading a 64-bit constant or a
label address uses the load-and-skip idiom:

```
ldr xN, 8          // xN = the word two halfwords ahead
b 12               // skip over it
.8byte value
```

## Frames

### `wain`

The loader passes the two arguments in `x0` and `x1`.

```
[x29 + 16]  saved x30 (return to loader)
[x29 + 8]   first parameter   (x0 on entry)
[x29 + 0]   second parameter  (x1 on entry)   ← x29 points here
[x29 - 8]   first local
[x29 - 16]  second local
...
```

Prologue: load the constants; push `x30`, `x0`, `x1`; `x29 = sp`; call
`init` with `x1` = array length (or 0 if the first parameter is `long`).
Epilogue: `x30 = [x29 + 16]`; pop `locals + 3` words; `br x30`.

### Other procedures

The **caller** saves its own frame and return address, pushes the arguments
left to right, and cleans everything up afterwards:

```
push x29
push x30
<evaluate arg 1>  push x0
...
<evaluate arg n>  push x0
ldr x5, 8 / b 12 / .8byte Fname
blr x5
add sp, sp, x8   (× n)      // drop the arguments
pop x30
pop x29
```

The **callee** therefore sees its last argument at `[sp, 0]` on entry:

```
[x29 + 8(n−1)]  parameter 1
...
[x29 + 0]       parameter n      ← x29 = sp on entry
[x29 − 8]       local 1
...
```

It sets `x29 = sp`, pushes its locals as they are declared, and before
`br x30` pops the locals. Recursion needs nothing special: every activation
has its own frame and the caller restores `x29`.

Procedure labels are prefixed with `F` (`Ftwice`) so user names can never
collide with runtime labels such as `print` or `new`.

## Expressions

Every expression leaves its value in `x0`. Binary operators:

```
<left>                 // x0 = left
push x0
<right>                // x0 = right
pop x1                 // x1 = left
op x0, x1, x0
```

| Rule | Instructions after the pop |
|---|---|
| `expr + term` (long + long) | `add x0, x1, x0` |
| `long* + long` | `mul x0, x0, x8` · `add x0, x1, x0` - pointer arithmetic scales by the word size |
| `long + long*` | `mul x1, x1, x8` · `add x0, x1, x0` |
| `expr − term` | `sub x0, x1, x0`, with the same scaling for `long* − long` |
| `long* − long*` | `sub x0, x1, x0` · `sdiv x0, x0, x8` - a difference in elements |
| `term * factor` | `mul x0, x1, x0` |
| `term / factor` | `sdiv x0, x1, x0` |
| `term % factor` | `sdiv x2, x1, x0` · `mul x2, x2, x0` · `sub x0, x1, x2` |

Factors:

| Rule | Code |
|---|---|
| `NUM` | load-and-skip the value |
| `NULL` | `add x0, x6, xzr` |
| `ID` | `ldur x0, [x29, offset]` |
| `* factor` | evaluate the pointer, then `ldur x0, [x0, 0]` |
| `& lvalue` | the lvalue's address (below) |
| `new long[expr]` | evaluate size into `x0`; save `x30`; call `new`; restore; if `x0 == 0` replace with `NULL` |
| `getchar()` | `ldur x0, [x11, 0]` |
| `f(args)` | the calling sequence above |

Lvalue addresses: a variable is `x29 + offset` (offset via load-and-skip,
then `add x0, x29, x1`); `*factor` is the pointer's value; parentheses
pass through. An assignment computes the address, pushes it, evaluates the
right side, pops the address into `x1`, and does `stur x0, [x1, 0]`.

## Statements and control flow

```
if (test) { A } else { B }        while (test) { A }
    <test, branch to elseN if false>   loopN:
    A                                   <test, branch to endloopN if false>
    b endifN                            A
  elseN:                                b loopN
    B                                 endloopN:
  endifN:
```

A test evaluates both sides, `cmp x1, x0`, then branches on the *negated*
condition. Pointers are compared unsigned:

| Test | `long` (signed) | `long*` (unsigned) |
|---|---|---|
| `==` / `!=` | `b.ne` / `b.eq` | same |
| `<` | `b.ge` | `b.hs` |
| `<=` | `b.gt` | `b.hi` |
| `>=` | `b.lt` | `b.lo` |
| `>` | `b.le` | `b.ls` |

`println(e)`: evaluate, save `x30`, `blr x10`, restore. `putchar(e)`:
evaluate, `stur x0, [x12, 0]`. `delete [] e`: evaluate; if `x0 == NULL`
skip; else save `x30`, call `delete`, restore.

## The runtime

The generated file begins with `.import print`, `.import init`,
`.import new`, `.import delete`. The linker resolves them against
`runtime/print.com` and `runtime/alloc.com`. `alloc.com` must be linked
last because `init` places the heap at *end of program + 8 × array length*,
growing toward the stack, which starts at the top of memory (`0x1000000`).

## Sample

For

```c
long twice(long x) { return x + x; }
long wain(long a, long b) {
  long c = 5;
  c = twice(a) + c;
  return c;
}
```

the assignment statement compiles to (comments are emitted by the compiler):

```
    // assignment
    ldr x1, 8    // &c
    b 12
    .8byte -8
    add x0, x29, x1
    stur x0, [sp, -8]        // push the address of c
    sub sp, sp, x8
    // call twice
    stur x29, [sp, -8]       // push x29
    sub sp, sp, x8
    stur x30, [sp, -8]       // push x30
    sub sp, sp, x8
    ldur x0, [x29, 8]    // a
    stur x0, [sp, -8]        // push the argument
    sub sp, sp, x8
    ldr x5, 8
    b 12
    .8byte Ftwice
    blr x5
    add sp, sp, x8           // drop the argument
    add sp, sp, x8           // pop x30
    ldur x30, [sp, -8]
    add sp, sp, x8           // pop x29
    ldur x29, [sp, -8]
    stur x0, [sp, -8]        // push twice(a)
    sub sp, sp, x8
    ldur x0, [x29, -8]    // c
    add sp, sp, x8           // pop twice(a) into x1
    ldur x1, [sp, -8]
    add x0, x1, x0
    add sp, sp, x8           // pop the address of c into x1
    ldur x1, [sp, -8]
    stur x0, [x1, 0]
```

and `twice` is:

```
Ftwice:
    add x29, sp, xzr    // frame pointer = top of the argument block
    // return expression
    ldur x0, [x29, 0]    // x
    stur x0, [sp, -8]
    sub sp, sp, x8
    ldur x0, [x29, 0]    // x
    add sp, sp, x8
    ldur x1, [sp, -8]
    add x0, x1, x0
    br x30
```

Run `bin/ncclc prog.nccl` to see the whole listing for any program.
