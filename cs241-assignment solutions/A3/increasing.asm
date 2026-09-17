
add x20, x30, xzr

ldr x21, 8
b 12
.8byte 1

ldr x28, 8
b 12
.8byte 8

add x22, x0, xzr //  start
mul x23, x1, x28 
add x23, x22, x23 // end

ldur x24, [x22, 0] // last element
add x22, x22, x28  // move starter

whileincreasing:
cmp x22, x23
b.ge success

ldur x25, [x22, 0] // new element
add x0, x24, xzr
add x1, x25, xzr

ldr x3, 8
b 12
.8byte compare
blr x3

cmp x0, x21
b.ne failed

add x24, x25, xzr
add x22, x22, x28  // move starter
b whileincreasing

failed:
sub x0, x0, x0
br x20

success:
add x0, x21, xzr
br x20

