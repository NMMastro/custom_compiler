// print two's complement number n in decimal
// Arguments:
//  x0 = n
// Prints in decimal
// Other registers:
//  x1: stdout address
//  x2: assci value 0
//  x3: quotient
//  x4: temperary values
//  x5: remainder
//  x6: assci value to be printed
//  x7: value of sp after storing
//  x8: value 8
//  x10: value 10


print:
    // ----- prologue -----
    stur x0, [sp, -8]
    stur x1, [sp, -16]
    stur x2, [sp, -24]
    stur x3, [sp, -32]
    stur x4, [sp, -40]
    stur x5, [sp, -48]
    stur x6, [sp, -56]
    stur x7, [sp, -64]
    stur x8, [sp, -72]
    stur x9, [sp, -80]
    stur x10, [sp, -88]

    ldr x1, 8
    b 12
    .8byte 88
    sub sp, sp, x1

    // ----- setup ------
    ldr x1, 8 // x1 = stdoutput
    b 12
    .8byte 0xc000000000010008

    ldr x2, 8 // x2 = assci value 0
    b 12
    .8byte 0x30

    ldr x10, 8 // x10 = 10
    b 12
    .8byte 10

    ldr x8, 8 // x8 = 8
    b 12
    .8byte 8

    add x7, sp, xzr

    // print '-' if negative number
    cmp x0, xzr
    b.ge whiledividing
    
    ldr x4, 8
    b 12
    .8byte 0x2D
    stur x4, [x1, 0]

    ldr x4, 8
    b 12
    .8byte -1
    mul x0, x0, x4


    whiledividing:

        udiv x3, x0, x10
        mul x4, x3, x10
        sub x5, x0, x4

        add x6, x2, x5
        stur x6, [sp, -8]
        sub sp, sp, x8

        add x0, x3, xzr

        cmp x0, xzr
        b.gt whiledividing

    printing:
        cmp sp, x7
        b.eq return

        ldur x4, [sp, 0]
        stur x4, [x1, 0]
        add sp, sp, x8
        b printing

    return:
        ldr x4, 8
        b 12
        .8byte 0x0A
        stur x4, [x1, 0]

        ldr x1, 8
        b 12
        .8byte 88
        add sp, sp, x1

        ldur x10, [sp, -88]
        ldur x9, [sp, -80]
        ldur x8, [sp, -72]
        ldur x7, [sp, -64]
        ldur x6, [sp, -56]
        ldur x5, [sp, -48]
        ldur x4, [sp, -40]
        ldur x3, [sp, -32]
        ldur x2, [sp, -24]
        ldur x1, [sp, -16]
        ldur x0, [sp, -8]

        br x30




    