ldr x1, 8
b 12
.8byte 0xc000000000010000

ldr x2, 8
b 12
.8byte 0xc000000000010008

ldr x3, 8
b 12
.8byte -1

ldr x8, 8
b 12
.8byte 8

add x29, sp, xzr

reading: 
ldur x4, [x1, 0] 
cmp x4, x3
b.eq printing

stur x4, [sp, -8]
sub sp, sp, x8
b reading

printing:
cmp sp, x29
b.eq return

ldur x4, [sp, 0]
stur x4, [x2, 0]
add sp, sp, x8
b printing

return:
br x30
