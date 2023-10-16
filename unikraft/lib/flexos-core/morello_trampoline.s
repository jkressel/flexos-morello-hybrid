.global compartment_trampoline
.type compartment_trampoline, "function"
.section .text
compartment_trampoline:

str c15, [sp, #-16]!
blr x11

msr CID_EL0, c19
ldr c15, [sp], #16
//clear sp
mov x11, #0
mov sp, x11
ldpbr c29, [c15]

.global compartment_trampoline_end
compartment_trampoline_end: