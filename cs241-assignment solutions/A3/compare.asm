compare:
cmp x0, x1
b.lt 12
sub x0, x0, x0
br x30
ldr x0, 8
b 12
.8byte 0x1
br x30
