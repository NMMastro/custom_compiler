//  Write a recursive function for:
//   - f(0,0) = 1
//   - f(i,0) = f(0,i) = 0   for i > 0
//   - f(n,k) = (n-1)*f(n-1,k) + f(n-1,k-1)   for n > 0, k > 0
// Arguments:
//  x0 = n
//  x1 = k
// Returns the result in x0
// registers:
//   x0: original n
//   x1: original k + temp vars
//   x2: copy of n
//   x3: copy of k
//   x4: 1
//   x5: n - 1
//   x6: (n-1) * f(n-1, k)
//   x30: return

stirling:
    // prologue
    stur x1,  [sp, -8]
    stur x2,  [sp, -16]
    stur x3,  [sp, -24]
    stur x4,  [sp, -32]
    stur x5,  [sp, -40]
    stur x6,  [sp, -48]
    stur x30, [sp, -56]

    // setup
    add x2, x0, xzr
    add x3, x1, xzr
    ldr x1, 8
    b 12
    .8byte 56
    sub sp, sp, x1
    ldr x4, 8
    b 12
    .8byte 1

    // base cases
    cmp x3, xzr // check if k == 0
    b.ne checkN
    cmp x2, xzr // given k == 0 check n == 0 ?
    b.ne returnZero
    add x0, x4, xzr // return 1 for f(0,0)
    b done

returnZero:
    sub x0, x0, x0
    b done

checkN:
    cmp x2, xzr  // given k > 0 check n == 0 ?
    b.eq returnZero

    // recursive case: n > 0, k > 0
    sub x5, x2, x4
    add x0, x5, xzr
    add x1, x3, xzr
    ldr x30, 8
    b 12
    .8byte stirling
    blr x30
    mul x6, x5, x0
    add x0, x5, xzr
    sub x1, x3, x4
    ldr x30, 8
    b 12
    .8byte stirling
    blr x30
    add x0, x6, x0

done:
    ldr x1, 8
    b 12
    .8byte 56
    add sp, sp, x1
    ldur x1,  [sp, -8]
    ldur x2,  [sp, -16]
    ldur x3,  [sp, -24]
    ldur x4,  [sp, -32]
    ldur x5,  [sp, -40]
    ldur x6,  [sp, -48]
    ldur x30, [sp, -56]
    br x30