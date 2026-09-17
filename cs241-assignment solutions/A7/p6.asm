// --------------- P6 CODE ---------------
// x8 -> constant 8
// x1 -> readWord
// x2 -> readHalfWord
// x3 -> printHex
// x20 -> endCode
// x19 -> currentPosition

// ----- prologue -----
// load and skip 8
ldr x8, 8
b 12
.8byte 8

// save x30
stur x30, [sp, -8]
sub sp, sp, x8

// Store readWord, ReadHalfWord, printHex
ldr x1, 8
b 12
.8byte readWord

ldr x2, 8
b 12
.8byte readHalfWord

ldr x3, 8
b 12
.8byte printHex

// ----- reading header -----

blr x2 // read break line (not needed)
blr x1 // read endModule (not needed)

// read end code and store it in x20
blr x1
add x20, x0, xzr

// current position in armcom
ldr x19, 8
b 12
.8byte 20

// ----- reading and printing -----
loop:
    cmp x19, x20
    b.ge end

    sub x4, x20, x19
    cmp x4, x8
    b.lt lastHalfWord

    blr x1
    blr x3

    add x19, x19, x8
    b loop


lastHalfWord:
    blr x2
    blr x3

end:
    add sp, sp, x8
    ldur x30, [sp, -8]
    br x30


readWord:
    stur x1, [sp, -8] // scratch ptr 1
    stur x2, [sp, -16] // Loop counter
    stur x8, [sp, -24] // 8
    stur x26, [sp, -32] // 256
    stur x3, [sp, -40] // Scratch to keep track of multiple
    ldr x1, 8
    b 12
    .8byte 0x28
    sub sp, sp, x1
    ldr x8, 8
    b 12
    .8byte 0x8
    ldr x26, 8
    b 12
    .8byte 256
    udiv x3, x8, x8

    add x2, x8, x8 // 2
    add x2, x2, x2 // 4
    add x2, x2, x2 // 8
    sub x0, x0, x0
    readWordLoop:
        cmp x2, xzr
        b.eq readWordLoopEnd
        stur x0, [sp, -8]
        stur x30, [sp, -16]
        sub sp, sp, x8
        sub sp, sp, x8

        ldr x1, 8
        b 12
        .8byte readByte
        blr x1

        mul x1, x0, x3
        add sp, sp, x8
        add sp, sp, x8
        ldur x30, [sp, -16]
        ldur x0, [sp, -8]
        add x0, x0, x1
        mul x3, x3, x26
        sub x2, x2, x8
        b readWordLoop
    readWordLoopEnd:
    ldr x1, 8
    b 12
    .8byte 0x28
    add sp, sp, x1
    ldur x1, [sp, -8] // scratch ptr 1
    ldur x2, [sp, -16] // Loop counter
    ldur x8, [sp, -24] // 8
    ldur x26, [sp, -32] // 256
    ldur x3, [sp, -40] // Scratch to keep track of multiple
    br x30

// -------------------

readHalfWord:
    stur x1, [sp, -8] // scratch ptr 1
    stur x2, [sp, -16] // Loop counter
    stur x8, [sp, -24] // 8
    stur x26, [sp, -32] // 256
    stur x3, [sp, -40] // Scratch to keep track of multiple
    ldr x1, 8
    b 12
    .8byte 0x28
    sub sp, sp, x1
    ldr x8, 8
    b 12
    .8byte 0x8
    ldr x26, 8
    b 12
    .8byte 256
    udiv x3, x8, x8

    add x2, x8, x8 // 2
    add x2, x2, x2 // 4
    sub x0, x0, x0
    readHalfWordLoop:
        cmp x2, xzr
        b.eq readHalfWordLoopEnd
        stur x0, [sp, -8]
        stur x30, [sp, -16]
        sub sp, sp, x8
        sub sp, sp, x8

        ldr x1, 8
        b 12
        .8byte readByte
        blr x1

        mul x1, x0, x3
        add sp, sp, x8
        add sp, sp, x8
        ldur x30, [sp, -16]
        ldur x0, [sp, -8]
        add x0, x0, x1
        mul x3, x3, x26
        sub x2, x2, x8
        b readHalfWordLoop
    readHalfWordLoopEnd:
    ldr x1, 8
    b 12
    .8byte 0x28
    add sp, sp, x1
    ldur x1, [sp, -8] // scratch ptr 1
    ldur x2, [sp, -16] // Loop counter
    ldur x8, [sp, -24] // 8
    ldur x26, [sp, -32] // 256
    ldur x3, [sp, -40] // Scratch to keep track of multiple
    br x30

// -------------------

readByte:
    stur x11, [sp, -8] // stdin
    stur x1, [sp, -16] // scratch ptr 1
    stur x2, [sp, -24] // scratch ptr 2
    ldr x1, 8
    b 12
    .8byte 0x18
    sub sp, sp, x1
    ldr x11, 8
    b 12
    .8byte 0xc000000000010000
    ldur x0, [x11, 0]
    ldr x1, 8
    b 12
    .8byte -1
    cmp x0, x1
    b.ne 8
    sub x0, x0, x0
    ldr x1, 8
    b 12
    .8byte 0x18
    add sp, sp, x1
    ldur x11, [sp, -8] // stdin
    ldur x1, [sp, -16] // scratch ptr 1
    ldur x2, [sp, -24] // scratch ptr 2
    br x30


printHex:
    stur x0, [sp, -8]
    stur x11, [sp, -16] // stdout
    stur x8, [sp, -24] // 8 (can be duped for 16)
    stur x20, [sp, -32] // Buffer base
    stur x1, [sp, -40] // Scratch ptr 1
    stur x2, [sp, -48] // Scratch ptr 2
    stur x6, [sp, -56] // Loop counter
    ldr x1, 8
    b 12
    .8byte 0x38
    sub sp, sp, x1

    ldr x11, 8
    b 12
    .8byte 0xc000000000010000
    ldr x8, 8
    b 12
    .8byte 8
    ldr x20, 8
    b 12
    .8byte bufferEnd

    ldr x6, 8
    b 12
    .8byte buffer
    recordLoop:
        cmp x6, x20
        b.gt printLoopStart
        // Take mod 16, encode, store at end of array, then dec
        add x1, x8, x8
        udiv x2, x0, x1
        mul x2, x2, x1 
        sub x2, x0, x2 // x2 = n mod 16
        mul x2, x2, x8 // Offset in bytes
        udiv x0, x0, x1 // Sneakily divide by 16

        ldr x1, 8
        b 12
        .8byte hexlut
        add x1, x1, x2
        ldur x2, [x1, 0]
        stur x2, [x20, 0] // Store in buffer
        sub x20, x20, x8 // Move ptr down
        b recordLoop

    printLoopStart:
    ldr x20, 8
    b 12
    .8byte bufferEnd
    ldr x2, 8
    b 12
    .8byte 0x20
    printLoop:
        cmp x6, x20
        b.gt printHexEnd
        ldur x1, [x20, -8]
        stur x1, [x11, 8]
        ldur x1, [x20, 0]
        stur x1, [x11, 8]
        stur x2, [x11, 8]
        sub x20, x20, x8
        sub x20, x20, x8
    b printLoop

    printHexEnd:
    ldr x1, 8
    b 12
    .8byte 0xA
    stur x1, [x11, 8]
    ldr x1, 8
    b 12
    .8byte 0x38
    add sp, sp, x1
    ldur x0, [sp, -8]
    ldur x11, [sp, -16] // stdout
    ldur x8, [sp, -24] // 8 (can be duped for 16)
    ldur x20, [sp, -32] // Buffer base
    ldur x1, [sp, -40] // Scratch ptr 1
    ldur x2, [sp, -48] // Scratch ptr 2
    ldur x6, [sp, -56] // Loop counter
    br x30

// Static buffer
buffer:
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
.8byte 0
bufferEnd:
.8byte 0

// Lookup table
hexlut:
.8byte 0x30 // 0
.8byte 0x31 // 1
.8byte 0x32 // 2
.8byte 0x33 // 3
.8byte 0x34 // 4
.8byte 0x35 // 5
.8byte 0x36 // 6
.8byte 0x37 // 7
.8byte 0x38 // 8 
.8byte 0x39 // 9
.8byte 0x61 // a
.8byte 0x62 // b
.8byte 0x63 // c
.8byte 0x64 // d
.8byte 0x65 // e
.8byte 0x66 // f
