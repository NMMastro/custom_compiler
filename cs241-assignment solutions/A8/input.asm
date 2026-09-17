b skip
.8byte 0

skip:
ldr x2, 8
b 12
.8byte 241

ldr x3, 8
b 12
.8byte 0x10004

stur x2, [x3, 0]
br x30