ldr x8, 8
b 12
.8byte 8
sub x20, x20, x20
ldr x10, 8
b 12
.8byte print
ldr x11, 8
b 12
.8byte 0xc000000000010000
ldr x12, 8
b 12
.8byte 0xc000000000010008
stur x30, [sp, -8]
sub sp, sp, x8
stur x0, [sp, -8]
sub sp, sp, x8
stur x1, [sp, -8]
sub sp, sp, x8
add x29, sp, xzr
ldr x0, 8
b 12
.8byte 241
stur x0, [sp, -8]
sub sp, sp, x8
ldur x0, [x29, 8]
blr x10
ldur x0, [x29, 0]
blr x10
ldur x0, [x29, -8]
blr x10
ldr x0, 8
b 12
.8byte 65
stur x0, [x12, 0]
ldr x0, 8
b 12
.8byte 10
stur x0, [x12, 0]
ldur x0, [x29, 8]
stur x0, [sp, -8]
sub sp, sp, x8
ldur x0, [x29, 0]
add sp, sp, x8
ldur x1, [sp, -8]
add x0, x1, x0
ldur x30, [x29, 16]
add sp, sp, x8
add sp, sp, x8
add sp, sp, x8
add sp, sp, x8
br x30
