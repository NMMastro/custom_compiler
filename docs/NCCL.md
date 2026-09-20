# The NCCL language

NCCL (Nate's Custom Coding Language) is a small C-like language: every valid
NCCL program is also a valid C++ fragment. It has two types (`long` and `long*`), procedures, `if`/`else`
and `while`, heap allocation with `new`/`delete []`, and character and
integer output. Its restrictions keep each compiler stage simple:

* All declarations come first in a procedure and are initialized with a
  constant (`0`, `241`, `NULL`) - never with an expression.
* Exactly one `return`, at the end of each procedure.
* `if` always has an `else`; both branches use braces; conditions are a
  single comparison (no `&&`, `||`, `!`).
* No unary minus (`0 - x`), no `[]` indexing (`*(a + i)`), no `break`.
* The last procedure is `wain(…, long)`; its first parameter is `long` or
  `long*` (an input array). Procedures may call themselves and any
  procedure declared *before* them, so there is no mutual recursion.

```c
// Sum an input array.
long wain(long* a, long n) {
  long i = 0;
  long sum = 0;
  while (i < n) {
    sum = sum + *(a + i);
    i = i + 1;
  }
  println(sum);
  return sum;
}
```

## Tokens

| Kind | Text |
|---|---|
| `ID` | a letter followed by letters/digits, not a keyword |
| `NUM` | `0`, or a nonzero digit followed by digits; must fit in a signed 64-bit integer |
| keywords | `wain long if else while println putchar getchar return NULL new delete` (kinds `WAIN`, `LONG`, …) |
| punctuation | `( ) { } [ ] , ;` → `LPAREN RPAREN LBRACE RBRACE LBRACK RBRACK COMMA SEMI` |
| operators | `= == != < > <= >= + - * / % &` → `BECOMES EQ NE LT GT LE GE PLUS MINUS STAR SLASH PCT AMP` |

Whitespace and `//` comments separate tokens and are discarded. Tokens are
found by **Simplified Maximal Munch**: take the longest prefix the DFA
accepts, emit it, repeat; if the DFA gets stuck in a non-accepting state the
input is rejected (there is no backtracking). So `a==b` is `ID EQ ID` and
`007` is three `NUM`s.

## Grammar

The start symbol is `procedures`. Terminals are in capitals.

```
procedures → procedure procedures | main
procedure  → LONG ID LPAREN params RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE
main       → LONG WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE
params     → ε | paramlist
paramlist  → dcl | dcl COMMA paramlist
type       → LONG | LONG STAR
dcls       → ε | dcls dcl BECOMES NUM SEMI | dcls dcl BECOMES NULL SEMI
dcl        → type ID
statements → ε | statements statement
statement  → lvalue BECOMES expr SEMI
           | IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE
           | WHILE LPAREN test RPAREN LBRACE statements RBRACE
           | PRINTLN LPAREN expr RPAREN SEMI
           | PUTCHAR LPAREN expr RPAREN SEMI
           | DELETE LBRACK RBRACK expr SEMI
test       → expr (EQ | NE | LT | LE | GE | GT) expr
expr       → term | expr PLUS term | expr MINUS term
term       → factor | term STAR factor | term SLASH factor | term PCT factor
factor     → ID | NUM | NULL | LPAREN expr RPAREN | AMP lvalue | STAR factor
           | NEW LONG LBRACK expr RBRACK | GETCHAR LPAREN RPAREN
           | ID LPAREN RPAREN | ID LPAREN arglist RPAREN
arglist    → expr | expr COMMA arglist
lvalue     → ID | STAR factor | LPAREN lvalue RPAREN
```

The grammar is SLR(1); the parser is driven by tables generated from it
(`src/nccl/grammar.h`). The tree the parser produces is written in the
`.tree` format: one line per node, in preorder, rule nodes as
`lhs rhs…` and leaves as `KIND lexeme`.

## Semantic rules

Names:

* Each procedure has its own variables (parameters and locals); a name may
  be declared at most once per procedure and must be declared in the
  procedure that uses it.
* A procedure may be declared once, and called only after its declaration
  (so recursion works, but a procedure cannot call one declared later).
* A variable and a procedure may share a name. Inside a procedure that
  declares variable `p`, `p` always means the variable - so `p(…)` there
  is an error, even if procedure `p` exists.

Types - every `expr`, `term`, `factor`, `lvalue`, `NUM`, `NULL`, and
variable `ID` gets a type:

| Expression | Type / requirement |
|---|---|
| `NUM`, `getchar()`, any procedure call | `long` |
| `NULL`, `new long[e]` (e: `long`) | `long*` |
| `*e` | `long`; e must be `long*` |
| `&lv` | `long*`; lv must be `long` |
| `e1 * e2`, `/`, `%` | both `long` |
| `long + long` | `long` |
| `long* + long`, `long + long*` | `long*` |
| `long − long` | `long` |
| `long* − long` | `long*` |
| `long* − long*` | `long` |
| `long − long*`, `long* + long*` | error |
| `f(a₁, …, aₙ)` | argument count and types must match `f`'s parameters exactly |

Statements and declarations:

* `lvalue = expr`: both sides the same type.
* `test`: both sides the same type (pointers are compared as unsigned).
* `println(e)`, `putchar(e)`: `long`. `delete [] e`: `long*`.
* `long x = NUM;` and `long* p = NULL;` only (not `long x = NULL;`).
* `wain`'s second parameter is `long`; every procedure returns `long`.

The type checker writes the annotated tree in the `.typed` format, which
is `.tree` with ` : long` or ` : long*` appended to every typed node.

## Runtime behaviour

* `println(x)` prints `x` in decimal with a newline; `putchar(x)` writes
  the low byte of `x`; `getchar()` returns the next input byte or −1.
* `new long[n]` returns `NULL` (−65536) if allocation fails.
* Dereferencing `NULL` crashes the program.
* `delete [] NULL` does nothing.
* Pointer arithmetic that leaves an array is allowed (and used by the
  tests); dereferencing such a pointer is undefined.

The `tests/type/` suite has one program per rule above - a negative case for
every error and positive cases for the edge cases (recursion, a parameter
sharing its procedure's name, all pointer-arithmetic forms).
