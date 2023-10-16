#include <flexos/impl/morello.h>

/* 
*   Arguments are expected in registers 0-7 allowing for 8 args
*   Indirect return pointer in x8
*   Number of arguments should be passed in x9
*   Target compartment ID should be in x10
*   Pointer to target function needs to be in x11
*   tsb_comp_to base addr should be in x12
*   tid in x13
*   DDC should be in c29
*/

.global switch_compartment
.type switch_compartment, "function"
.section compartment_switchers, "ax", %progbits
switch_compartment:

//  Put DDC into the DDC register
    mrs c17, ddc
    stp c17, clr, [sp, #-32]!
    // mov x17, sp
    // sub x17, x17, #16
    // str x17, [sp, #-8]!


    msr ddc, c29

//  size of compartment caps struct
    mov x14, #32

//  get offset of correct compartment to switch to
    mul x14, x14, x10

//  load the ddc for the new compartment
    ldr c15, [x29, x14]

//  load the pcc for the new compartment
    add x14, x14, #16
    ldr c16, [x29, x14]

////////////////////////////////////////////////
// c15 = compartment ddc
// c16 = compartment pcc
////////////////////////////////////////////////

//  set new compartment ddc
    msr ddc, c15

    mrs c19, CID_EL0
    msr CID_EL0, c10

    mov x15, sp
    cvt c17, c17, x15
    scbnds c15, c17, #32
    scvalue c15, c15, x17
    seal c15, c15, lpb

//  size of tsb
    mov x14, #16

//  offset of tsb
    mul x14, x14, x13

//  tsb base
    add x12, x12, x14

//  load sp
    ldr x14, [x12]
    mov sp, x14
//  load fp
    ldr x14, [x12, #8]
    mov fp, x14
//  set new sp and fp

//  branch, we don't want to return
    br c16
    



.global switch_compartment_end
switch_compartment_end:
