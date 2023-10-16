#ifndef FLEXOS_MORELLO_IMPL_H
#define FLEXOS_MORELLO_IMPL_H

#include <flexos/impl/morello.h>
#include <uk/page.h>
#include <uk/arch/lcpu.h>


static inline
int uk_thread_get_tid(void)
{
	unsigned long sp = ukarch_read_sp();
	return *((int *) round_pgup((unsigned long) ((sp & STACK_MASK_TOP) + 1)));
}

#define IS_CAP(arg)	(sizeof(arg) == 16)
#define IS_DWORD(arg)	(sizeof(arg) == 8)

#define __flexos_morello_move_argument1_cap(arg) 	\
do {	\
	__asm__ volatile(	\
	"mov c0, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument2_cap(arg) 	\
do {	\
	__asm__ volatile (	\
	"mov c1, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument3_cap(arg) 	\
do {	\
	__asm__ volatile (	\
	"mov c2, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument4_cap(arg) 	\
do {	\
	__asm__ volatile(	\
	"mov c3, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument5_cap(arg) 	\
do {	\
	__asm__ volatile(	\
	"mov c4, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument6_cap(arg) 	\
do {	\
	__asm__ volatile(	\
	"mov c5, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument7_cap(arg) 	\
do {	\
	__asm__ volatile(	\
	"mov c6, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument1_int(arg) 	\
do {	\
	__asm__ volatile (	\
	"mov x0, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument2_int(arg) 	\
do {	\
	__asm__ volatile(	\
	"mov x1, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument3_int(arg) 	\
do {	\
	__asm__ (	\
	"mov x2, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
); 	\
} while (0)

#define __flexos_morello_move_argument4_int(arg) 	\
do {	\
	__asm__ (	\
	"mov x3, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
); 	\
} while (0)

#define __flexos_morello_move_argument5_int(arg) 	\
do {	\
	__asm__ (	\
	"mov x4, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument6_int(arg) 	\
do {	\
	__asm__ (	\
	"mov x5, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument7_int(arg) 	\
do {	\
	__asm__ (	\
	"mov x6, %0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument1_word(arg) 	\
do {	\
	__asm__ volatile (	\
	"mov w0, %w0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument2_word(arg) 	\
do {	\
	__asm__ volatile(	\
	"mov w1, %w0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument3_word(arg) 	\
do {	\
	__asm__ (	\
	"mov w2, %w0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
); 	\
} while (0)

#define __flexos_morello_move_argument4_word(arg) 	\
do {	\
	__asm__ (	\
	"mov w3, %w0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
); 	\
} while (0)

#define __flexos_morello_move_argument5_word(arg) 	\
do {	\
	__asm__ (	\
	"mov w4, %w0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument6_word(arg) 	\
do {	\
	__asm__ (	\
	"mov w5, %w0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define __flexos_morello_move_argument7_word(arg) 	\
do {	\
	__asm__ (	\
	"mov w6, %w0\n"	\
	:				\
	: "r"(arg)	\
	: "c1", "c0", "c2", "c3", "c4", "c5", "c6", "c7", "x8", "x9", "x10", "x11", "x12", "x13", "x14"	\
);	\
} while (0)

#define flexos_morello_move_arg_int_into_reg(arg, number)	\
	switch (number)	\
	{	\
	case 1:	\
		__flexos_morello_move_argument1_int(arg);	\
		break;	\
	case 2:	\
		__flexos_morello_move_argument2_int(arg);	\
		break;	\
	case 3:	\
		__flexos_morello_move_argument3_int(arg);	\
		break;	\
	case 4:	\
		__flexos_morello_move_argument4_int(arg);	\
		break;	\
	case 5:	\
		__flexos_morello_move_argument5_int(arg);	\
		break;	\
	case 6:	\
		__flexos_morello_move_argument6_int(arg);	\
		break;	\
	case 7:	\
		__flexos_morello_move_argument7_int(arg);	\
		break;	\
	default:	\
		break;	\
	}	

#define flexos_morello_move_arg_cap_into_reg(arg, number)	\
	switch (number)	\
	{	\
	case 1:	\
		__flexos_morello_move_argument1_cap(arg);	\
		break;	\
	case 2:	\
		__flexos_morello_move_argument2_cap(arg);	\
		break;	\
	case 3:	\
		__flexos_morello_move_argument3_cap(arg);	\
		break;	\
	case 4:	\
		__flexos_morello_move_argument4_cap(arg);	\
		break;	\
	case 5:	\
		__flexos_morello_move_argument5_cap(arg);	\
		break;	\
	case 6:	\
		__flexos_morello_move_argument6_cap(arg);	\
		break;	\
	case 7:	\
		__flexos_morello_move_argument7_cap(arg);	\
		break;	\
	default:	\
		break;	\
	}	

#define flexos_morello_move_arg_word_into_reg(arg, number)	\
	switch (number)	\
	{	\
	case 1:	\
		__flexos_morello_move_argument1_word(arg);	\
		break;	\
	case 2:	\
		__flexos_morello_move_argument2_word(arg);	\
		break;	\
	case 3:	\
		__flexos_morello_move_argument3_word(arg);	\
		break;	\
	case 4:	\
		__flexos_morello_move_argument4_word(arg);	\
		break;	\
	case 5:	\
		__flexos_morello_move_argument5_word(arg);	\
		break;	\
	case 6:	\
		__flexos_morello_move_argument6_word(arg);	\
		break;	\
	case 7:	\
		__flexos_morello_move_argument7_word(arg);	\
		break;	\
	default:	\
		break;	\
	}	





#define __flexos_morello_gate0(key_from, key_to, f_ptr)\
do {									\
__asm__ volatile (	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from)))	\
	: "x20", "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate0_r(key_from, key_to, retval_ptr, f_ptr)\
do {									\
__asm__ volatile (	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str x0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate1_i_instrumented(key_from, key_to, f_ptr, arg1)\
do {									\
__asm__ volatile (	\
	"isb\n"\
	"mrs x15, PMCCNTR_EL0\n"\
	"str x15, [%9, #56]\n"\
	"stp c28, c27, [sp, #-32]!\n"\
	"stp c26, c25, [sp, #-32]!\n"\
	"stp c24, c23, [sp, #-32]!\n"\
	"stp c22, c21, [sp, #-32]!\n"\
	"mov x28, %9\n"\
	"isb\n"\
	"mrs x21, PMCCNTR_EL0\n"\
	"mov x0, %8\n"\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"isb\n"\
	"mrs x22, PMCCNTR_EL0\n"\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"isb\n"\
	"mrs x26, PMCCNTR_EL0\n"\
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	"isb\n"\
	"mrs x27, PMCCNTR_EL0\n"\
	"str x21, [x28]\n"\
	"str x22, [x28, #8]\n"\
	"str x23, [x28, #16]\n"\
	"str x24, [x28, #24]\n"\
	"str x25, [x28, #32]\n"\
	"str x26, [x28, #40]\n"\
	"str x27, [x28, #48]\n"\
	"ldp c22, c21, [sp], #32\n"		\
	"ldp c24, c23, [sp], #32\n"		\
	"ldp c26, c25, [sp], #32\n"		\
	"ldp c28, c27, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))), "r"(arg1), "r"(cycles)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate1_i(key_from, key_to, f_ptr, arg1)\
do {									\
__asm__ volatile (	\
	"mov x0, %8\n"\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))), "r"(arg1), "r"(cycles)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate1_r(key_from, key_to, retval_ptr, f_ptr, arg1)\
do {									\
	\
	if (IS_CAP(arg1)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg1, 1);	\
 	} else if (IS_DWORD(arg1))  {	\
 		flexos_morello_move_arg_int_into_reg(arg1, 1);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg1, 1);	\
	}\
 	\
	/* todo \
* - backup registers we need first, come back to this */ \
\
\
__asm__ volatile (	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str x0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"	\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate1_rword(key_from, key_to, retval_ptr, f_ptr, arg1)\
do {									\
	\
	if (IS_CAP(arg1)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg1, 1);	\
 	} else if (IS_DWORD(arg1))  {	\
 		flexos_morello_move_arg_int_into_reg(arg1, 1);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg1, 1);	\
	}\
 	\
	/* todo \
* - backup registers we need first, come back to this */ \
\
\
__asm__ volatile (	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"mov x17, sp\n"	\
	"add sp, sp, #288\n"	\
	"str w0, [x14]\n"	\
	"mov sp, x17\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate1_rword_i(key_from, key_to, retval_ptr, f_ptr, arg1)\
do {									\
__asm__ volatile (	\
	"mov x0, %9\n"\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str w0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr), "r"(arg1)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate1_rword_c(key_from, key_to, retval_ptr, f_ptr, arg1)\
do {									\
__asm__ volatile (	\
	"mov c0, %9\n"\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str w0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr), "r"(arg1)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)

/*There is an issue where if you enter a compartment from a different place you may break the sp/fp, this needs fixing someday*/
#define __flexos_morello_gate2_ii(key_from, key_to, f_ptr, arg1, arg2)\
do {									\
	/* todo \
* - backup registers we need first, come back to this */ \
\
\
__asm__ volatile (	\
	"mov x0, %8\n"	\
	"mov x1, %9\n"	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))), "r"(arg1), "r"(arg2)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate2_ci(key_from, key_to, f_ptr, arg1, arg2)\
do {									\
	/* todo \
* - backup registers we need first, come back to this */ \
\
\
__asm__ volatile (	\
	"mov c0, %8\n"	\
	"mov x1, %9\n"	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))), "r"(arg1), "r"(arg2)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate2_r(key_from, key_to, retval_ptr, f_ptr, arg1, arg2)\
do {									\
	\
	if (IS_CAP(arg1)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg1, 1);	\
 	} else if (IS_DWORD(arg1))  {	\
 		flexos_morello_move_arg_int_into_reg(arg1, 1);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg1, 1);	\
	}\
 	\
	\
	if (IS_CAP(arg2)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg2, 2);	\
 	} else if (IS_DWORD(arg2))  {	\
 		flexos_morello_move_arg_int_into_reg(arg2, 2);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg2, 2);	\
	}\
 	\
	/* todo \
* - backup registers we need first, come back to this */ \
\
\
__asm__ volatile (	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str x0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18", "x30"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate2_r_word_ii(key_from, key_to, retval_ptr, f_ptr, arg1, arg2)\
do {									\
__asm__ volatile (	\
	"mov x0, %9\n"\
	"mov x1, %10\n"\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str w0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr), "r"(arg1), "r"(arg2)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17","x18", "x30"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate3_r_pii(key_from, key_to, retval_ptr, f_ptr, arg1, arg2, arg3)\
do {									\
__asm__ volatile (	\
	"mov x0, %9\n"\
	"mov x1, %10\n"\
	"mov x2, %11\n"\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str x0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr), "r"(arg1), "r"(arg2), "r"(arg3)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17","x18", "x30"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate3_r_word_pii(key_from, key_to, retval_ptr, f_ptr, arg1, arg2, arg3)\
do {									\
__asm__ volatile (	\
	"mov x0, %9\n"\
	"mov x1, %10\n"\
	"mov x2, %11\n"\
	"stp x20, x30, [sp, #-16]!\n"		\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str w0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	"ldp x20, x30, [sp], #16\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr), "r"(arg1), "r"(arg2), "r"(arg3)	\
	: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17","x18"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate4(key_from, key_to, f_ptr, arg1, arg2, arg3, arg4)\
do {									\
	\
	if (IS_CAP(arg1)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg1, 1);	\
 	} else if (IS_DWORD(arg1))  {	\
 		flexos_morello_move_arg_int_into_reg(arg1, 1);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg1, 1);	\
	}\
	if (IS_CAP(arg2)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg2, 2);	\
 	} else if (IS_DWORD(arg2))  {	\
 		flexos_morello_move_arg_int_into_reg(arg2, 2);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg2, 2);	\
	}\
	if (IS_CAP(arg3)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg3, 3);	\
 	} else if (IS_DWORD(arg3))  {	\
 		flexos_morello_move_arg_int_into_reg(arg3, 3);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg3, 3);	\
	}\
 	\
	if (IS_CAP(arg4)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg4, 4);	\
 	} else if (IS_DWORD(arg4))  {	\
 		flexos_morello_move_arg_int_into_reg(arg4, 4);	\
 	} else {\
		flexos_morello_move_arg_word_into_reg(arg4, 4);	\
	}\
 	\
 	\
	/* todo \
* - backup registers we need first, come back to this */ \
\
\
__asm__ volatile (	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from)))	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17","x18", "x30"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate4_variant1(key_from, key_to, f_ptr, arg1, arg2, arg3, arg4)\
do {									\
__asm__ volatile (	\
	"mov c0, %8\n"\
	"mov c1, %9\n"\
	"mov c2, %10\n"\
	"mov x3, %11\n"\
	"stp x20, x30, [sp, #-16]!\n"		\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	"ldp x20, x30, [sp], #16\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)	\
	: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17","x18"\
);	\
\
\
\
} while (0)

#define __flexos_morello_gate4_r_word_iiii(key_from, key_to, retval_ptr, f_ptr, arg1, arg2, arg3, arg4)\
do {									\
__asm__ volatile (	\
	"mov x0, %9\n"\
	"mov x1, %10\n"\
	"mov x2, %11\n"\
	"mov x3, %12\n"\
	"stp x17, x30, [sp, #-16]!\n"		\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str w0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	"ldp x17, x30, [sp], #16\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16"\
);	\
\
\
\
} while (0)





#define __flexos_morello_gate4_r_cici(key_from, key_to, retval_ptr, f_ptr, arg1, arg2, arg3, arg4)\
do {									\
__asm__ volatile (	\
	"mov c0, %9\n"\
	"mov x1, %10\n"\
	"mov c2, %11\n"\
	"mov x3, %12\n"\
	"stp x20, x30, [sp, #-16]!\n"		\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str x0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	"ldp x20, x30, [sp], #16\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4)	\
	: "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "x18"\
);	\
\
\
\
} while (0)


#define __flexos_morello_gate7_r(key_from, key_to, retval_ptr, f_ptr, arg1, arg2, arg3, arg4, arg5, arg6, arg7)\
do {									\
	\
	 	if (IS_CAP(arg1)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg1, 1);	\
 	} else {	\
 		flexos_morello_move_arg_int_into_reg(arg1, 1);	\
 	}	\
 	\
 	if (IS_CAP(arg2)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg2, 2);	\
 	} else {	\
 		flexos_morello_move_arg_int_into_reg(arg2, 2);	\
 	}	\
 	\
 	if (IS_CAP(arg3)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg3, 3);	\
 	} else {	\
 		flexos_morello_move_arg_int_into_reg(arg3, 3);	\
 	}	\
 	\
 	if (IS_CAP(arg4)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg4, 4);	\
 	} else {	\
 		flexos_morello_move_arg_int_into_reg(arg4, 4);	\
 	}	\
 	\
 	if (IS_CAP(arg5)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg5, 5);	\
 	} else {	\
 		flexos_morello_move_arg_int_into_reg(arg5, 5);	\
 	}	\
 	\
 	if (IS_CAP(arg6)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg6, 6);	\
 	} else {	\
 		flexos_morello_move_arg_int_into_reg(arg6, 6);	\
 	}	\
 	\
 	if (IS_CAP(arg7)) {	\
 		flexos_morello_move_arg_cap_into_reg(arg7, 7);	\
 	} else {	\
 		flexos_morello_move_arg_int_into_reg(arg7, 7);	\
 	}	\
	/* todo \
* - backup registers we need first, come back to this */ \
\
\
__asm__ volatile (	\
	"stp c29, c19, [sp, #-32]!\n"		\
/* x12 will hold tsb sp and x13 will hold tsb fp */ 	\
	"mov x13, %0\n"	\
	"mul x11, x13, %1\n"	\
	"add x11, x11, %2\n"	\
	"ldp x12, x15, [x11]\n"	\
	"mov x14, %8\n"\
	"stp x12, x15, [sp, #-16]!\n"	\
	"stp x11, x14, [sp, #-16]!\n"\
	/* backup the current sp and fp */ 	\
/*tsb_comp ## key_from[tid].sp = register asm("sp");*/	\
/*tsb_comp ## key_from[tid].bp = register asm("fp");*/	\
/* x11 hold the base of tsb_comp ## key_from as calculated above */ 	\
	"mov x10, sp\n"	\
/*This is to allow us to store things like ddc, return address */	\
	"sub x10, x10, #48\n"	\ 
	"stp x10, fp, [x11]\n"	\
	"mov x20, x11 \n"	\
	/* Now we need to load the dest compartment id into a register and the number of arguments*/	\
/* TODO: tying to load the address of tsb comp for the target may not work, may need to revist this in the future*/	\
	"mov x10, %3\n"	\
	"mov x9, %4\n"	\
	"mov x11, %5\n"	\
	"mov x12, %6\n"	\
	\
	\
	\
	\
	\
	\
	\
	/* Load the switcher caps and branch to switcher using unsealing instruction ldpblr */	\
	"ldr c14, [%7]\n"	\
	"ldpblr c29, [c14]\n" \
	"msr ddc, c29\n"\
	"mov x11, x20\n"	\
	"ldr x11, [x11]\n"	\
	"mov sp, x11\n"	\
	"ldp x11, x14, [sp, #48]!\n"\
	"add sp, sp, #16\n"	\
	"ldp x12, fp, [x11]\n"	\
	"ldp x12, x13, [sp], #16\n"	\
	"stp x12, x13, [x11]\n"\
	"str w0, [x14]\n"	\
	\
	\
	\
	"ldp c29, c19, [sp], #32\n"		\
	:	\
	: "r"(uk_thread_get_tid()), "r" (sizeof(struct uk_thread_status_block)), "r" ((tsb_comp ## key_from)), "i"(key_to),	"i"(1), "r"(f_ptr), "r"(tsb_comp ## key_to), "r"((uintptr_t *)(&(switcher_call_comp ## key_from))),	"r"(&retval_ptr)	\
	: "x20","x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17","x18", "x30"\
);	\
\
\
\
} while (0)






#define _flexos_morello_gate(N, key_from, key_to, fname, ...)		\
do {									\
	UK_CTASSERT(N <= 6);						\
	__flexos_morello_gate ## N (key_from, key_to, fname __VA_OPT__(,) __VA_ARGS__); \
} while (0)

#define _flexos_morello_gate_r(N, key_from, key_to, retval, fname, ...)\
do {									\
	UK_CTASSERT(N <= 7);						\
	__flexos_morello_gate ## N ## _r (key_from, key_to, retval, fname __VA_OPT__(,) __VA_ARGS__); \
} while (0)


#define MORELLO_LOAD_SHARED_DATA(ptr_to_load, var_to_put_it_in) \
do { \
	__asm__ (\
	"scvalue c17, c18, %1\n"\
	"ldr %0, [c17]\n"\
	: "=&r"(var_to_put_it_in)\
	: "r"(ptr_to_load)\
	: "memory", "c17"\
	);\
} while (0)

#define MORELLO_STORE_SHARED_DATA(ptr_to_store, val_to_store) \
do { \
	__asm__ (\
	"scvalue c17, c18, %1\n"\
	"str %0, [c17]\n"\
	: \
	: "r"(val_to_store), "r"(ptr_to_store)\
	: "memory", "c17"\
	);\
} while (0)


#endif

